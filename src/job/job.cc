/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "job.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

#include "bc/binary.h"
#include "bc/index.h"
#include "compiler.h"
#include "database.h"
#include "db/driver.h"
#include "db/read-only-driver.h"
#include "exec.h"
#include "filter/writer.h"
#include "flint/bc.h"
#include "flint/ls.h"
#include "job.h"
#include "lo/layout.h"
#include "lo/layout_loader.h"
#include "phsp.h"
#include "sedml.h"
#include "solver.h"
#include "task.h"
#include "task/config-reader.h"

namespace flint {
namespace job {

bool Job(int id,
		 const char *task_dir,
		 const char *job_dir,
		 task::Task &task,
		 const fppp::Option *fppp_option,
		 std::vector<double> *data,
		 const char *output_file,
		 const task::ConfigReader &reader,
		 sqlite3 *db)
{
	const int kShort = 64;
	const int kLong = 96;

	boost::system::error_code ec;
	// ensure the job directory
	boost::filesystem::create_directories(job_dir, ec);
	if (ec) {
		std::cerr << "failed to create directory: " << ec << std::endl;
		return false;
	}

	if (!task::Timer(reader.length(), reader.step(), data->data()))
		return false;

	char output_data_file[kLong];
	sprintf(output_data_file, "%s/output-data", job_dir);
	char output_history_file[kLong];
	sprintf(output_history_file, "%s/output-history", job_dir);

	job::Option option;
	option.id = id;
	option.dir = job_dir;
	option.input_data = data;
	option.input_history_file = nullptr;
	option.output_data_file = output_data_file;
	option.output_history_file = output_history_file;
	option.fppp_option = fppp_option;

	char isdh_file[kShort];
	sprintf(isdh_file, "%s/isdh", task_dir);
	boost::filesystem::copy_file(isdh_file, output_file, ec);
	if (ec) {
		std::cerr << "failed to copy "
			 << isdh_file
			 << " to "
			 << output_file
			 << ": " << ec << std::endl;
		return false;
	}
	FILE *ofp = std::fopen(output_file, "ab");
	if (!ofp) {
		std::perror(output_file);
		return false;
	}
	// write initial values only when output_start_time is 0.
	if (task.output_start_time == 0) {
		if (!task.writer->Write(data->data(), ofp)) {
			std::fclose(ofp);
			return false;
		}
	}
	option.output_fp = ofp;

	bool r;
	if (reader.GetMethod() == compiler::Method::kArk) {
		r = solver::Solve(db, solver::Method::kArk, task, option);
	} else {
		r = job::Evolve(task, option);
	}
	std::fclose(ofp);
	return r;
}

}
}
