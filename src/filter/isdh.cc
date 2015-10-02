/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "filter.hh"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/uuid/uuid_io.hpp>

#include "lo.pb.h"

#include "filter/filter_loader.h"
#include "isdf/isdf.h"

using std::cerr;
using std::cin;
using std::endl;
using std::memcpy;
using std::string;
using std::vector;

namespace flint {
namespace filter {

namespace {

class Filter {
public:
	Filter(const Filter &) = delete;
	Filter &operator=(const Filter &) = delete;

	Filter()
		: num_objs_(),
		  num_bytes_descs_(),
		  num_bytes_units_(),
		  descriptions_(),
		  units_()
	{}

	void ReadHeader(int /*size*/) const {
		// ignore header
	}

	void ReadColumn(lo::Column *column) {
		boost::uuids::uuid u;
		memcpy(&u, column->uuid().data(), u.size());
		string s;
		if (u.is_nil()) {
			s = column->name();
		} else {
			s = to_string(u) + ":" + column->name();
			if (column->has_label()) s += "@" + column->label();
		}
		num_objs_ += column->size();
		num_bytes_descs_ += (sizeof(std::uint32_t)+s.size()) * column->size();
		num_bytes_units_ += (sizeof(std::uint32_t)+column->unit().size()) * column->size();
		for (int i=0;i<column->size();i++) {
			descriptions_.push_back(s);
			units_.push_back(column->unit());
		}
	}

	void Write(std::ofstream *ofs) const {
		isdf::ISDFHeader header;
		header.num_objs = num_objs_;
		header.num_bytes_comment = 0;
		header.num_bytes_descs = num_bytes_descs_;
		header.num_bytes_units = num_bytes_units_;

		char buf[sizeof(isdf::ISDFHeader)];
		memcpy(buf, &header, sizeof(header));
		ofs->write(buf, sizeof(header));

		for (vector<string>::const_iterator it=descriptions_.begin();it!=descriptions_.end();++it) {
			std::uint32_t len = it->size();
			memcpy(buf, &len, sizeof(len));
			ofs->write(buf, sizeof(len));
			ofs->write(it->c_str(), len);
		}
		for (vector<string>::const_iterator it=units_.begin();it!=units_.end();++it) {
			std::uint32_t len = it->size();
			memcpy(buf, &len, sizeof(len));
			ofs->write(buf, sizeof(len));
			ofs->write(it->c_str(), len);
		}
	}

private:
	std::uint32_t num_objs_;
	std::uint32_t num_bytes_descs_;
	std::uint32_t num_bytes_units_;
	vector<string> descriptions_;
	vector<string> units_;
};

}

bool Isdh(const char *filter_file, const char *output_file)
{
	std::unique_ptr<Filter> filter(new Filter());
	{
		std::unique_ptr<FilterLoader> loader(new FilterLoader(filter_file));
		if (!loader->Load(filter.get())) return false;
	}
	std::ofstream ofs(output_file, std::ios::out|std::ios::binary);
	if (!ofs.is_open()) {
		cerr << "could not open output file: " << output_file << endl;
		return false;
	}
	filter->Write(&ofs);
	ofs.close();
	return true;
}

}
}
