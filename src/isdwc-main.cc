/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <boost/program_options.hpp>
#include "isdf/reader.h"

namespace po = boost::program_options;

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::ifstream;
using std::istream;
using std::ios;
using std::ofstream;
using std::ostream;
using std::string;

namespace {

class Counter {
public:
	Counter() : nrows_() {}

	boost::uint32_t nrows() const {return nrows_;}

	int GetStep(size_t, const char *) {
		nrows_++;
		return 1;
	}

private:
	boost::uint32_t nrows_;
};

int ReadAndCount(bool columns, istream *is, ostream *os)
{
	isdf::Reader reader;
	if (!reader.ReadHeader(is)) return EXIT_FAILURE;
	if (!reader.SkipComment(is)) return EXIT_FAILURE;

	if (columns) {
		*os << reader.num_objs() << endl;
		return EXIT_SUCCESS;
	}

	if (!reader.SkipDescriptions(is)) return EXIT_FAILURE;
	if (!reader.SkipUnits(is)) return EXIT_FAILURE;
	Counter counter;
	if (!reader.ReadSteps(counter, is)) return EXIT_FAILURE;
	*os << reader.num_objs() << " " << counter.nrows() << endl;
	return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[])
{
	po::options_description opts("options");
	po::positional_options_description popts;
	po::variables_map vm;
	string input_file, output_file;
	int print_help = 0;

	opts.add_options()
		("columns,c", "Print the column counts only")
		("help,h", "Show this message")
		("output,o", po::value<string>(&output_file), "Output file name")
		("input", po::value<string>(&input_file), "Input file name");
	popts.add("input", 1);

	try {
		po::store(po::command_line_parser(argc, argv).options(opts).positional(popts).run(), vm);
		po::notify(vm);
		if (vm.count("help")) print_help = 1;
	} catch (const po::error &) {
		print_help = 2;
	}
	if (print_help != 0) {
		cerr << "usage: isdwc [OPTIONS] [PATH]" << endl;
		cerr << opts;
		return (print_help == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	bool columns = (vm.count("columns") > 0);
	int r;
	if (vm.count("output")) {
		ofstream ofs(output_file.c_str(), ios::out|ios::binary);
		if (!ofs.is_open()) {
			cerr << "could not open output file: " << output_file << endl;
			return EXIT_FAILURE;
		}
		if (vm.count("input")) {
			ifstream ifs(input_file.c_str(), ios::in|ios::binary);
			if (!ifs.is_open()) {
				cerr << "could not open input file: " << input_file << endl;
				return EXIT_FAILURE;
			}
			r = ReadAndCount(columns, &ifs, &ofs);
			ifs.close();
		} else {
			r = ReadAndCount(columns, &cin, &ofs);
		}
		ofs.close();
	} else {
		if (vm.count("input")) {
			ifstream ifs(input_file.c_str(), ios::in|ios::binary);
			if (!ifs.is_open()) {
				cerr << "could not open input file: " << input_file << endl;
				return EXIT_FAILURE;
			}
			r = ReadAndCount(columns, &ifs, &cout);
			ifs.close();
		} else {
			r = ReadAndCount(columns, &cin, &cout);
		}
	}
	return r;
}
