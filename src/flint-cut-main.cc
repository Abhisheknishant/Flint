/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "lo.pb.h"

#include "filter.hh"

namespace po = boost::program_options;

using std::cerr;
using std::endl;
using std::fclose;
using std::fopen;
using std::perror;
using std::string;

int main(int argc, char *argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	po::options_description opts("options");
	po::positional_options_description popts;
	po::variables_map vm;
	string filter_file, input_file, output_file;
	int print_help = 0;

	opts.add_options()
		("filter", po::value<string>(&filter_file), "Input filter file")
		("input", po::value<string>(&input_file), "Input file")
		("output", po::value<string>(&output_file), "Output file")
		("help,h", "Show this message");
	popts.add("filter", 1).add("input", 1).add("output", 1);

	try {
		po::store(po::command_line_parser(argc, argv).options(opts).positional(popts).run(), vm);
		po::notify(vm);
		if (vm.count("help") > 0) {
			print_help = 1;
		} else if ( vm.count("filter") == 0 ||
					vm.count("input") == 0 ||
					vm.count("output") == 0 ) {
			print_help = 2;
		}
	} catch (const po::error &) {
		print_help = 2;
	}
	if (print_help) {
		cerr << "usage: " << argv[0] << " FILTER INPUT OUTPUT" << endl;
		cerr << opts;
		return (print_help == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	FILE *ifp, *ofp;
	ifp = fopen(input_file.c_str(), "rb");
	if (!ifp) {
		perror(input_file.c_str());
		return EXIT_FAILURE;
	}
	ofp = fopen(output_file.c_str(), "wb");
	if (!ofp) {
		perror(output_file.c_str());
		fclose(ifp);
		return EXIT_FAILURE;
	}
	bool r = filter::Cut(filter_file.c_str(), ifp, ofp);
	fclose(ofp);
	fclose(ifp);

	google::protobuf::ShutdownProtobufLibrary();

	return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
