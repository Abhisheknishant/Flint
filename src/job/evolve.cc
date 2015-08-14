/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "job.hh"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#define BOOST_DATE_TIME_NO_LIB
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/noncopyable.hpp>

#include "bc.pb.h"

#include "bc/bc_loader.h"
#include "bc/index.h"
#include "bc/locater.h"
#include "bc/mounter.h"
#include "bc/pack.h"
#include "db/read-only-driver.hh"
#include "filter/cutter.h"
#include "lo/layout.h"
#include "lo/layout_loader.h"
#include "runtime/history.h"
#include "runtime/processor.h"
#include "runtime/timeseries.h"
#include "task/config-reader.hh"
#include "ts.hh"

using std::cerr;
using std::endl;
using std::fclose;
using std::fopen;
using std::fputc;
using std::fread;
using std::fwrite;
using std::string;

namespace flint {
namespace job {

namespace {

class Executor : boost::noncopyable {
public:
	explicit Executor(size_t layer_size)
	: layer_size_(layer_size),
	  data_(NULL),
	  prev_(NULL),
	  tmp_(NULL),
	  history_(NULL) {}

	void set_data(double *data) {data_ = data;}
	void set_prev(double *prev) {prev_ = prev;}
	void set_tmp(double *tmp) {tmp_ = tmp;}
	void set_history(History *history) {history_ = history;}

	bool Lb(const bc::Lb &lb, int offset) {
		int k = offset + lb.so();
		return history_[k].Lookback(lb, data_[kIndexTime], tmp_);
	}

	bool Load(const bc::Load &load, int offset) {
		switch (load.lo()) {
		case -1:
			tmp_[load.a()] = prev_[offset + load.so()];
			break;
		case -2:
			tmp_[load.a()] = data_[load.so()];
			break;
		default:
			{
				int k = offset + load.so() + (layer_size_ * load.lo());
				tmp_[load.a()] = data_[k];
			}
			break;
		}
		return true;
	}

	double Store(const bc::Store &store, int offset) {
		assert(store.lo() >= 0);
		int k = offset + store.so() + (layer_size_ * store.lo());
		double v = tmp_[store.a()];
		data_[k] = v;
		return v;
	}

	bool Reduce(const ReductionUnit &ru) {
		return ru(data_);
	}

private:
	size_t layer_size_;
	double *data_;
	double *prev_;
	double *tmp_;
	History *history_;
};

class PExecutor : boost::noncopyable {
public:
	explicit PExecutor(size_t layer_size)
	: layer_size_(layer_size),
	  data_(NULL),
	  prev_(NULL),
	  color_(NULL),
	  tmp_(NULL),
	  history_(NULL) {}

	void set_data(double *data) {data_ = data;}
	void set_prev(double *prev) {prev_ = prev;}
	void set_color(int *color) {color_ = color;}
	void set_tmp(double *tmp) {tmp_ = tmp;}
	void set_history(History *history) {history_ = history;}

	bool Lb(const bc::Lb &lb, int offset) {
		int k = offset + lb.so();
		return history_[k].Lookback(lb, prev_[kIndexTime], tmp_);
	}

	bool Load(const bc::Load &load, int offset) {
		switch (load.lo()) {
		case -1:
			tmp_[load.a()] = prev_[offset + load.so()];
			break;
		case -2:
			tmp_[load.a()] = prev_[load.so()];
			break;
		default:
			{
				assert(false);
			}
			break;
		}
		return true;
	}

	double Store(const bc::Store &store, int offset) {
		assert(store.lo() == 0);
		int k = offset + store.so();
		double v = tmp_[store.a()];
		data_[k] = v;
		color_[k] = 1;
		return v;
	}


	void Communicate(const FlowInboundMap *inbound) {
		for (auto it=inbound->cbegin();it!=inbound->cend();++it) {
			int dst = it->first;
			auto &sources = it->second->second;
			double d;
			switch (it->second->first) {
			case Reduction::kUnspecified:
				assert(false);
				break;

#define DATA_AT(i) (color_[i]) ? data_[i] : prev_[i]

			case Reduction::kSum:
				d = 0;
				for (auto src : sources)
					d += DATA_AT(src);
				break;
			case Reduction::kMax:
				{
					auto sit = sources.cbegin();
					auto seit = sources.cend();
					assert(sit != seit);
					d = DATA_AT(*sit);
					while (++sit != seit)
						d = std::max(d, DATA_AT(*sit));
				}
				break;
			case Reduction::kMin:
				{
					auto sit = sources.cbegin();
					auto seit = sources.cend();
					assert(sit != seit);
					d = DATA_AT(*sit);
					while (++sit != seit)
						d = std::min(d, DATA_AT(*sit));
				}
				break;
			case Reduction::kMean:
				{
					auto sit = sources.cbegin();
					auto seit = sources.cend();
					assert(sit != seit);
					d = DATA_AT(*sit);
					while (++sit != seit)
						d += DATA_AT(*sit);
					d /= sources.size();
				}
				break;

#undef DATA_AT

			case Reduction::kDegree:
				d = sources.size();
				break;
			}
			data_[dst] = d;
			color_[dst] = 1;
		}
	}

	void Flush() {
		// write back to data
		for (size_t i=kOffsetBase;i<layer_size_;i++) {
			if (color_[i]) prev_[i] = data_[i];
		}
		// clear cache and data
		std::memset(color_, 0, layer_size_ * sizeof(int));
		std::memset(data_, 0, layer_size_ * sizeof(double));
	}

private:
	size_t layer_size_;
	double *data_;
	double *prev_;
	int *color_;
	double *tmp_;
	History *history_;
};

bool CreateStatusFile(const char *status_file)
{
	FILE *fp = fopen(status_file, "wb");
	if (!fp) {
		cerr << "could not open " << status_file << endl;
		return false;
	}
	if (fputc('\0', fp) == EOF) {
		fclose(fp);
		cerr << "failed to write byte to " << status_file << endl;
		return false;
	}
	fclose(fp);
	return true;
}

bool LoadData(const char *input_data_file, size_t layer_size, double *data)
{
	FILE *fp = fopen(input_data_file, "rb");
	if (!fp) {
		cerr << "could not open " << input_data_file << endl;
		return false;
	}
	if (fread(data, sizeof(double), layer_size, fp) != layer_size) {
		fclose(fp);
		cerr << "missing or short input data" << endl;
		return false;
	}
	fclose(fp);
	return true;
}

bool SaveData(const char *output_data_file, size_t layer_size, double *data)
{
	FILE *fp = fopen(output_data_file, "wb");
	if (!fp) {
		cerr << "could not open " << output_data_file << endl;
		return false;
	}
	if (fwrite(data, sizeof(double), layer_size, fp) != layer_size) {
		fclose(fp);
		cerr << "failed to write output data" << endl;
		return false;
	}
	fclose(fp);
	return true;
}

}

bool Evolve(sqlite3 *db,
			const char *layout_file,
			const char *bc_file,
			FILE *output_fp,
			const Option &option)
{
	size_t granularity = 1;
	{
		task::ConfigReader reader(db);
		if (!reader.Read())
			return false;
		granularity = reader.granularity();
	}

	bool with_filter = option.filter_file != NULL;
	bool with_pre = option.pre_file != NULL;
	bool with_post = option.post_file != NULL;
	bool with_control = option.control_file != NULL;
	bool with_status = option.status_file != NULL;

	// load layout at first
	std::unique_ptr<Layout> layout(new Layout);
	{
		std::unique_ptr<LayoutLoader> loader(new LayoutLoader(layout_file));
		if (!loader->Load(layout.get())) return false;
	}
	size_t layer_size = layout->Calculate();

	// load filter next
	std::unique_ptr<filter::Writer> writer;
	if (with_filter) {
		filter::Cutter cutter;
		if (!cutter.Load(option.filter_file, layer_size)) return false;
		writer.reset(cutter.CreateWriter());
	}

	std::unique_ptr<Executor> executor(new Executor(layer_size));
	std::unique_ptr<Processor> processor(new Processor(layout.get(), layer_size));
	std::unique_ptr<PExecutor> preexecutor(new PExecutor(layer_size));
	std::unique_ptr<Processor> preprocessor(new Processor(layout.get(), layer_size));
	std::unique_ptr<PExecutor> postexecutor(new PExecutor(layer_size));
	std::unique_ptr<Processor> postprocessor(new Processor(layout.get(), layer_size));

	// load bc next
	int nol = 0;
	{
		std::unique_ptr<BcLoader> loader(new BcLoader(bc_file));
		if (!loader->Load(&nol, processor.get())) return false;
	}
	if (nol <= 0) {
		cerr << "invalid nol: " << nol << endl;
		return false;
	}
	// load preprocess bytecode if specified
	if (with_pre) {
		std::unique_ptr<BcLoader> loader(new BcLoader(option.pre_file));
		if (!loader->Load(NULL, preprocessor.get())) return false;
	}
	// load postprocess bytecode if specified
	if (with_post) {
		std::unique_ptr<BcLoader> loader(new BcLoader(option.post_file));
		if (!loader->Load(NULL, postprocessor.get())) return false;
	}

	processor->CalculateCodeOffset();
	if (with_pre) preprocessor->CalculateCodeOffset();
	if (with_post) postprocessor->CalculateCodeOffset();

	// replace nominal location in bytecode
	if (!processor->SolveLocation()) {
		return false;
	}
	if (with_pre && !preprocessor->SolveLocation()) {
		return false;
	}
	if (with_post && !postprocessor->SolveLocation()) {
		return false;
	}

	// arrange input timeseries data
	std::unique_ptr<TimeseriesVector> tv;

	std::unique_ptr<FlowInboundMap> inbound(new FlowInboundMap);
	{
		tv.reset(new TimeseriesVector);
		if (!ts::LoadTimeseriesVector(db, tv.get()))
			return false;
		processor->set_tv(tv.get());
		if (with_pre) preprocessor->set_tv(tv.get());
		if (with_post) postprocessor->set_tv(tv.get());

		if (!LoadFlows(db, inbound.get()))
			return false;
	}

	if (!processor->SolveDependencies(nol, inbound.get(), true)) {
		return false;
	}

	// arrange previous data space
	std::unique_ptr<double[]> prev(new double[layer_size * nol]()); // default-initialized
	if (option.input_data_file != NULL) { // fill the first layer with input
		if (!LoadData(option.input_data_file, layer_size, prev.get())) return false;
	}
	executor->set_prev(prev.get());
	preexecutor->set_prev(prev.get());

	// arrange data space
	std::unique_ptr<double[]> data(new double[layer_size * nol]()); // default-initialized
	executor->set_data(data.get());
	postexecutor->set_prev(data.get());

	// copy time, dt, end and seed to data
	std::memcpy(data.get(), prev.get(), kOffsetBase * sizeof(double));
	// constant disposition
	{
		std::set<int> constants;
		layout->CollectConstant(1, layer_size, &constants);
		for (auto offset : constants) {
			double val = prev[offset];
			for (int i=1;i<nol;i++) { // except the 1st layer
				prev[offset + i*layer_size] = val;
			}
			for (int i=0;i<nol;i++) {
				data[offset + i*layer_size] = val;
			}
		}
	}

	// arrange work space
	std::unique_ptr<double[]> work(new double[layer_size]()); // default-initialized
	preexecutor->set_data(work.get());
	postexecutor->set_data(work.get());

	// arrange color space
	std::unique_ptr<int[]> color(new int[layer_size]()); // default-initialized
	preexecutor->set_color(color.get());
	postexecutor->set_color(color.get());

	// arrange history
	std::unique_ptr<History[]> history(new History[layer_size]);
	if (!layout->SpecifyCapacity(layer_size, history.get())) {
		return false;
	}
	if (option.input_history_file != NULL) {
		HistoryLoader loader(option.input_history_file);
		if (!loader.Load(layer_size, history.get())) return false;
	}
	executor->set_history(history.get());
	preexecutor->set_history(history.get());
	postexecutor->set_history(history.get());

	// calculate max number of data of block
	int max_nod = processor->GetMaxNumberOfData();
	if (with_pre) max_nod = std::max(max_nod, preprocessor->GetMaxNumberOfData());
	if (with_post) max_nod = std::max(max_nod, postprocessor->GetMaxNumberOfData());

	// initialize temporary data
	std::unique_ptr<double[]> tmp(new double[max_nod]);
	executor->set_tmp(tmp.get());
	processor->set_tmp(tmp.get());
	if (with_pre) {
		preexecutor->set_tmp(tmp.get());
		preprocessor->set_tmp(tmp.get());
	}
	if (with_post) {
		postexecutor->set_tmp(tmp.get());
		postprocessor->set_tmp(tmp.get());
	}

	// initialize pseudo random number generator
	std::unique_ptr<std::mt19937> rng(new std::mt19937(static_cast<int>(data[kIndexSeed])));
	processor->set_rng(rng.get());
	if (with_pre) preprocessor->set_rng(rng.get());
	if (with_post) postprocessor->set_rng(rng.get());

	std::unique_ptr<boost::interprocess::file_mapping> control_fm;
	std::unique_ptr<boost::interprocess::mapped_region> control_region;
	if (with_control) {
		control_fm.reset(new boost::interprocess::file_mapping(option.control_file, boost::interprocess::read_only));
		control_region.reset(new boost::interprocess::mapped_region(*control_fm, boost::interprocess::read_only));
	}
	char control;

	if (with_status) {
		if (!CreateStatusFile(option.status_file)) return false;
	}
	char p, q = 0;

	size_t g = 0;

	bool result = true;
	// execute bytecode
	do {
		// push into history
		for (size_t k=0;k<layer_size; k++) {
			history[k].Insert(prev[kIndexTime], prev[k]);
		}

		// advance step
		if (with_pre) {
			if (!preprocessor->Execute(preexecutor.get(), inbound.get())) {
				result = false;
				break;
			}
		}
		if (!processor->Process(executor.get())) {
			result = false;
			break;
		}
		// one time step forward
		data[kIndexTime] += prev[kIndexDt];
		double last_time = data[kIndexTime];
		if (with_post) {
			if (!postprocessor->Execute(postexecutor.get(), inbound.get())) {
				result = false;
				break;
			}
		}

		if (granularity <= 1 || ++g == granularity) {
			if (with_filter) {
				if (!writer->Write(data.get(), output_fp)) {
					return false;
				}
			} else {
				// output the first layer of data
				if (fwrite(data.get(), sizeof(double), layer_size, output_fp) != layer_size) {
					cerr << "failed to write output" << endl;
					return false;
				}
			}
			g = 0;
		}

		if (with_status) {
			if (data[kIndexEnd] <= 0) {
				cerr << "non-positive end time: " << data[kIndexEnd] << endl;
				return false;
			}
			p = static_cast<char>(100 * (data[kIndexTime] / data[kIndexEnd]));
			if (p != q) {
				FILE *fp = fopen(option.status_file, "r+b");
				if (fp) {
					fwrite(&p, 1, 1, fp);
					fclose(fp);
				}
				q = p;
			}
		}

		if (with_control) {
			memcpy(&control, control_region->get_address(), 1);
			if (control == 1) {
				break;
			}
		}

		// update prev via double buffering
		data.swap(prev);
		executor->set_prev(prev.get());
		if (with_pre) preexecutor->set_prev(prev.get());
		executor->set_data(data.get());
		if (with_post) postexecutor->set_prev(data.get());
		data[kIndexTime] = last_time;
	} while (data[kIndexTime] < data[kIndexEnd]);
	fflush(output_fp);

	if (option.output_data_file != NULL) {
		if (!SaveData(option.output_data_file, layer_size, data.get())) return false;
	}
	if (option.output_history_file != NULL) {
		HistoryDumper dumper(option.output_history_file);
		if (!dumper.Dump(layer_size, history.get())) return false;
	}

	return result;
}

}
}
