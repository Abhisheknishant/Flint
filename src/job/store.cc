/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "job.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "lo.pb.h"

#include "bc/index.h"
#include "db/statement-driver.h"
#include "lo/layout_loader.h"

using std::cerr;
using std::endl;
using std::fclose;
using std::fopen;
using std::fread;
using std::fseek;
using std::make_pair;
using std::memcpy;
using std::perror;
using std::printf;
using std::sscanf;
using std::string;
using std::strlen;

namespace flint {
namespace job {

namespace {

class FormatLoader : db::StatementDriver {
public:
	// Note that db is for read only.
	explicit FormatLoader(sqlite3 *db)
		: db::StatementDriver(db, "SELECT format FROM model")
	{}

	// the return value should be freed by caller.
	char *Load()
	{
		int e;
		e = sqlite3_step(stmt());
		if (e != SQLITE_ROW) {
			cerr << "failed to step statement: " << e << endl;
			return nullptr;
		}
		const char *f = (const char *)sqlite3_column_text(stmt(), 0);
		size_t len = strlen(f);
		char *format = new char[len+1];
		strcpy(format, f);
		return format;
	}
};

typedef std::unordered_map<int, int> TargetMap;

class SourceLayout {
public:
	SourceLayout(const SourceLayout &) = delete;
	SourceLayout &operator=(const SourceLayout &) = delete;

	SourceLayout() {}

	void AddTrack(std::unique_ptr<lo::Track> &&track) {
		tv_.push_back(std::move(track));
	}

	void AddSector(std::unique_ptr<lo::Sector> &&sector) {
		(void)sector; // nothing to do
	}

	void AddData(std::unique_ptr<lo::Data> &&data) {
		dv_.push_back(std::move(data));
	}

	int CollectTargets(TargetMap *tm) const {
		int si = 0;
		int di = 0;
		int pos = kOffsetBase;
		for (const auto &tp : tv_) {
			int nos = tp->nos();
			int nod = tp->nod();
			int sie = si + nos;
			int dib = di;
			int die = di + nod;

			while (si++ < sie) {
				di = dib;
				while (di < die) {
					const auto &dp = dv_.at(di++);
					switch (dp->type()) {
					case lo::S:
						if (std::strncmp(dp->name().c_str(), "phsp:target", 11) == 0) {
							const char *nstr = dp->name().c_str();
							int target_id = std::atoi(&nstr[11]);
							tm->insert(make_pair(target_id, pos));
						}
						break;
					default:
						cerr << "unexpected data type: " << dp->type() << endl;
						return -1;
					}
					pos += dp->col() * dp->row();
				}
			}
		}
		return pos;
	}

private:
	typedef std::vector<std::unique_ptr<lo::Track> > TrackVector;
	typedef std::vector<std::unique_ptr<lo::Data> > DataVector;

	TrackVector tv_;
	DataVector dv_;
};

typedef std::unordered_map<boost::uuids::uuid,
						   std::unordered_map<string, double>,
						   boost::hash<boost::uuids::uuid> > TargetValueMap;

class TargetLoader : db::StatementDriver {
public:
	// Note that db is for read only.
	explicit TargetLoader(sqlite3 *db)
		: db::StatementDriver(db, "SELECT uuid, id FROM phsp_targets WHERE rowid = ?")
	{}

	bool Load(const TargetMap &tm, const double *data, TargetValueMap *tvm) {
		boost::uuids::uuid u;
		for (TargetMap::const_iterator it=tm.begin();it!=tm.end();++it) {
			int e = sqlite3_bind_int(stmt(), 1, it->first);
			if (e != SQLITE_OK) {
				cerr << "failed to bind rowid: " << e << endl;
				return false;
			}
			e = sqlite3_step(stmt());
			if (e != SQLITE_ROW) {
				cerr << "missing row with rowid " << it->first << " in phsp_targets" << endl;
				return false;
			}
			const void *uuid = sqlite3_column_blob(stmt(), 0);
			assert(uuid);
			memcpy(&u, uuid, u.size());
			(*tvm)[u].insert(make_pair((const char *)sqlite3_column_text(stmt(), 1), data[it->second]));
			sqlite3_reset(stmt());
		}
		return true;
	}
};

class TargetLayout {
public:
	TargetLayout(const TargetLayout &) = delete;
	TargetLayout &operator=(const TargetLayout &) = delete;

	TargetLayout() {}

	void AddTrack(std::unique_ptr<lo::Track> &&track) {
		tv_.push_back(std::move(track));
	}

	void AddSector(std::unique_ptr<lo::Sector> &&sector) {
		sv_.push_back(std::move(sector));
	}

	void AddData(std::unique_ptr<lo::Data> &&data) {
		dv_.push_back(std::move(data));
	}

	bool Rewrite(const char *format, const TargetValueMap &tvm, FILE *fp) const {
		std::unique_ptr<char[]> buf(new char[32]); // FIXME
		int si = 0;
		int di = 0;
		if (fseek(fp, kOffsetBase*sizeof(double), SEEK_SET) != 0) {
			return false;
		}
		for (const auto &tp : tv_) {
			int nos = tp->nos();
			int nod = tp->nod();
			int sie = si + nos;
			int dib = di;
			int die = di + nod;

			while (si < sie) {
				const auto &sp = sv_.at(si++);
				boost::uuids::uuid su;
				memcpy(&su, sp->id().data(), su.size());
				di = dib;
				while (di < die) {
					const auto &dp = dv_.at(di++);
					int data_size = dp->col() * dp->row();
					switch (dp->type()) {
					case lo::S:
					case lo::X:
						{
							TargetValueMap::const_iterator it = tvm.find(su);
							if (it == tvm.end()) {
								if (fseek(fp, data_size*sizeof(double), SEEK_CUR) != 0) {
									return false;
								}
							} else {
								std::unordered_map<string, double>::const_iterator mit;
								if (strcmp("phml", format) == 0) {
									sprintf(buf.get(), "%d", dp->id());
									mit = it->second.find(buf.get());
								} else {
									mit = it->second.find(dp->name());
								}
								if (mit == it->second.end()) {
									if (fseek(fp, data_size*sizeof(double), SEEK_CUR) != 0) {
										return false;
									}
								} else {
									// TODO: support for non-scalar values
									double value = mit->second;
									if (std::fwrite(&value, sizeof(double), 1u, fp) != 1u)
										return false;
									int size = data_size - 1;
									if (std::fseek(fp, size*sizeof(double), SEEK_CUR) != 0)
										return false;
								}
							}
						}
						break;
					default:
						if (fseek(fp, data_size*sizeof(double), SEEK_CUR) != 0) {
							return false;
						}
						break;
					}
				}
			}
		}
		return true;
	}

private:
	typedef std::vector<std::unique_ptr<lo::Track> > TrackVector;
	typedef std::vector<std::unique_ptr<lo::Sector> > SectorVector;
	typedef std::vector<std::unique_ptr<lo::Data> > DataVector;

	TrackVector tv_;
	SectorVector sv_;
	DataVector dv_;
};

}

bool Store(sqlite3 *db,
		   const char *source_layout_file, const char *source_data_file,
		   const char *target_layout_file, const char *target_data_file)
{
	SourceLayout source_layout;
	{
		std::unique_ptr<LayoutLoader> loader(new LayoutLoader(source_layout_file));
		if (!loader->Load(&source_layout)) return false;
	}
	TargetMap tm;
	int source_layer_size = source_layout.CollectTargets(&tm);
	if (source_layer_size <= 0) {
		return false;
	}

	std::unique_ptr<double[]> data(new double[source_layer_size]);
	FILE *fp = fopen(source_data_file, "rb");
	if (!fp) {
		perror(source_data_file);
		return false;
	}
	if (fread(data.get(), sizeof(double), source_layer_size, fp) != static_cast<size_t>(source_layer_size)) {
		cerr << "could not read data with size: " << source_layer_size << endl;
		fclose(fp);
		return false;
	}
	fclose(fp);

	std::unique_ptr<char[]> format;
	TargetValueMap tvm;
	{
		// check model's format
		{
			FormatLoader loader(db);
			format.reset(loader.Load());
			if (!format) return false;
		}
		{
			TargetLoader loader(db);
			if (!loader.Load(tm, data.get(), &tvm))
				return false;
		}
	}

	TargetLayout target_layout;
	{
		std::unique_ptr<LayoutLoader> loader(new LayoutLoader(target_layout_file));
		if (!loader->Load(&target_layout)) return false;
	}
	fp = fopen(target_data_file, "r+b");
	if (!fp) {
		perror(target_data_file);
		return false;
	}
	if (!target_layout.Rewrite(format.get(), tvm, fp)) {
		fclose(fp);
		return false;
	}
	fclose(fp);

	return true;
}

}
}
