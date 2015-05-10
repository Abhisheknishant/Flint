/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "db/driver.h"
#include "db/query.h"

using std::cerr;
using std::endl;
using std::strcmp;

namespace {

void Usage()
{
	cerr << "usage: flint-singleton DB" << endl;
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc != 2) {
		Usage();
		return EXIT_FAILURE;
	}
	if (strcmp("-h", argv[1]) == 0 || strcmp("--help", argv[1]) == 0) {
		Usage();
		return EXIT_SUCCESS;
	}
	boost::scoped_ptr<db::Driver> driver(new db::Driver(argv[1]));
	sqlite3 *db = driver->db();
	char *em;
	int e;
	if (!BeginTransaction(db))
		return EXIT_FAILURE;
	e = sqlite3_exec(db, "CREATE VIEW IF NOT EXISTS spaces AS SELECT '00000000-0000-0000-0000-000000000000', 'default'",
					 NULL, NULL, &em);
	if (e != SQLITE_OK) {
		cerr << "failed to create spaces: " << e << ": " << em << endl;
		sqlite3_free(em);
		return EXIT_FAILURE;
	}
	e = sqlite3_exec(db, "CREATE VIEW IF NOT EXISTS scopes AS SELECT '00000000-0000-0000-0000-000000000000', '00000000-0000-0000-0000-000000000000'",
					 NULL, NULL, &em);
	if (e != SQLITE_OK) {
		cerr << "failed to create scopes: " << e << ": " << em << endl;
		sqlite3_free(em);
		return EXIT_FAILURE;
	}
	if (!CreateTable(db, "names", "(space_id TEXT, type TEXT, id INTEGER, name TEXT, unit TEXT, capacity REAL)"))
		return EXIT_FAILURE;
	if (!CreateTable(db, "time_unit", "(name TEXT)"))
		return EXIT_FAILURE;
	return CommitTransaction(db) ? EXIT_SUCCESS : EXIT_FAILURE;
}
