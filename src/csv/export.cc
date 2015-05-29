/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "export.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
#define BOOST_DATE_TIME_NO_LIB
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/scoped_array.hpp>
#include "isdf/isdf.h"

using std::cerr;
using std::endl;
using std::strcmp;
using std::memcpy;
using std::vector;
using std::fwrite;

namespace {

// Returns true if it is vaild, false otherwise.
// This can modify the input 'column'.
// Note that the output argument 'unit' is non-null if a unit is found.
bool DecomposeColumn(char *column, char **unit)
{
	char *p = column;
	char c;
	while ( (c = *p) ) {
		if (c != ' ') {
			++p;
			continue;
		}
		char *s = p++;
		if ( (c = *p++) == '\0' ) {
			cerr << "invalid column found: " << column << endl;
			return false;
		}
		if (c != '(') {
			cerr << "invalid column found: " << column << endl;
			return false;
		}
		if ( (c = *p++) == '\0' ) {
			cerr << "invalid column found: " << column << endl;
			return false;
		}
		if (c == ')') {
			cerr << "empty unit found: " << column << endl;
			return false;
		}
		while ( (c = *p++) ) {
			if (c != ')') continue;
			if (*p--  != '\0') {
				cerr << "trailing chars after ')': " << column << endl;
				return false;
			}
			*p = '\0'; // replace ')' with '\0'
			*s++ = '\0'; // replace ' ' with '\0'
			*unit = ++s; // set begin of unit
			return true;
		}
		cerr << "missing ')': " << column << endl;
		return false;
	}
	*unit = NULL; // no unit included
	return true;
}

typedef std::map<boost::uint32_t, char *> UnitMap;
typedef std::map<boost::uint32_t, boost::uint32_t> UnitLengthMap;

} // namespace

bool ExportIsdFromCsv(const boost::filesystem::path &input_path,
					  const boost::filesystem::path &output_path)
{
	std::string input_file = input_path.string();
	boost::interprocess::file_mapping ifm(input_file.c_str(), boost::interprocess::read_only);
	boost::interprocess::mapped_region imr(ifm, boost::interprocess::read_only);
	char *addr = static_cast<char *>(imr.get_address());
	size_t s = imr.get_size();
	vector<size_t> bol;
	vector<size_t> eol;
	bol.push_back(0);
	for (size_t i=0; i<s; i++) {
		switch (addr[i]) {
		case '\r':
			eol.push_back(i);
			break;
		case '\n':
			if (i+1 < s) {
				bol.push_back(i+1);
			}
			break;
		default:
			break;
		}
	}
	if (bol.size() != eol.size()) {
		if (eol.size() == 0) {
			cerr << "The input file seems Unix-style; LF found, but no CR." << endl;
		} else {
			cerr << "BOL/EOL mismatch: " << bol.size() << " vs " << eol.size() << endl;
		}
		return false;
	}

	size_t first_line_len = eol[0];
	boost::scoped_array<char> first_line(new char[first_line_len + 1]);
	memcpy(first_line.get(), addr, first_line_len);
	first_line[first_line_len] = '\0';
	vector<char *> columns;
	columns.push_back(first_line.get());
	for (size_t i=0; i<first_line_len; i++) {
		if (first_line[i] == ',') {
			first_line[i] = '\0';
			if (i+1 < first_line_len) {
				columns.push_back(first_line.get()+i+1);
			}
		}
	}

	boost::uint32_t num_cols = static_cast<boost::uint32_t>(columns.size());

	UnitMap units;
	UnitLengthMap unit_len;
	for (boost::uint32_t i=0;i<num_cols;i++) {
		char *unit = NULL;
		if (!DecomposeColumn(columns[i], &unit)) return false;
		if (!unit) continue;
		units.insert(std::make_pair(i, unit));
		size_t len = strlen(unit);
		assert(len);
		unit_len.insert(std::make_pair(i, static_cast<boost::uint32_t>(len)));
	}
	assert(units.size() == unit_len.size());
	bool no_units = (units.size() == 0);

	boost::uint32_t num_bytes_descs = 4 * num_cols;
	vector<boost::uint32_t> desc_len;
	for (boost::uint32_t i=0; i<num_cols; i++) {
		size_t len = strlen(columns[i]);
		if (len == 0) {
			cerr << "empty column name at " << i << endl;
			return false;
		}
		boost::uint32_t u32len = static_cast<boost::uint32_t>(len);
		desc_len.push_back(u32len);
		num_bytes_descs += u32len;
	}

	boost::uint32_t num_bytes_units = (no_units) ? 0 : 4 * num_cols;
	if (!no_units) {
		for (boost::uint32_t i=0;i<num_cols;i++) {
			UnitLengthMap::const_iterator it = unit_len.find(i);
			if (it != unit_len.end()) {
				num_bytes_units += it->second;
			}
		}
	}

	isdf::ISDFHeader header;
	header.num_objs = num_cols;
	header.num_bytes_comment = 0;
	header.num_bytes_descs = num_bytes_descs;
	header.num_bytes_units = num_bytes_units;

	std::string output_file = output_path.string();
	FILE *ofp = std::fopen(output_file.c_str(), "wb");
	if (!ofp) {
		std::perror(output_file.c_str());
		return false;
	}
	if (fwrite(&header, sizeof(header), 1, ofp) != 1) {
		cerr << "failed to write header" << endl;
		return false;
	}
	// descriptions
	for (boost::uint32_t i=0; i<num_cols; i++) {
		boost::uint32_t u32len = desc_len[i];
		if (fwrite(&u32len, 4, 1, ofp) != 1) {
			cerr << "failed to write length" << endl;
			return false;
		}
		if (fwrite(columns[i], u32len, 1, ofp) != 1) {
			cerr << "failed to write description" << endl;
			return false;
		}
	}
	if (!no_units) { // units
		for (boost::uint32_t i=0;i<num_cols;i++) {
			UnitMap::const_iterator it = units.find(i);
			if (it == units.end()) {
				boost::uint32_t u32len = 0;
				if (fwrite(&u32len, 4, 1, ofp) != 1) {
					cerr << "failed to write unit's length" << endl;
					return false;
				}
			} else {
				boost::uint32_t u32len = unit_len[i];
				if (fwrite(&u32len, 4, 1, ofp) != 1) {
					cerr << "failed to write unit's length" << endl;
					return false;
				}
				if (fwrite(it->second, u32len, 1, ofp) != 1) {
					cerr << "failed to write unit" << endl;
					return false;
				}
			}
		}
	}
	// body
	for (size_t i=1; i<bol.size(); i++) {
		char *p = addr + bol[i];
		boost::uint32_t nc = 0;
		do {
			char *q;
			double d = strtod(p, &q);
			if (p == q) {
				cerr << "invalid double: " << (addr + bol[i]) << endl;
				return false;
			}
			if (fwrite(&d, sizeof(d), 1, ofp) != 1) {
				cerr << "failed to write value" << endl;
				return false;
			}
			if (addr + eol[i] <= q) {
				break;
			}
			p = q + 1;
		} while (++nc < num_cols);
	}
	std::fclose(ofp);
	return true;
}
