/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "read-only-driver.hh"

#include <cstdio>
#include <cstdlib>
#include <iostream>

using std::cerr;
using std::endl;

namespace db {

ReadOnlyDriver::ReadOnlyDriver(const char *filename)
	: db_(NULL)
{
	if (sqlite3_open_v2(filename, &db_, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		cerr << "failed to open database for read-only: " << filename << endl;
		exit(EXIT_FAILURE);
	}
}

ReadOnlyDriver::~ReadOnlyDriver()
{
	sqlite3_close(db_);
}

sqlite3 *ReadOnlyDriver::db()
{
	return db_;
}

}
