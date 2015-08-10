/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "phml.hh"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "database.h"
#include "db/driver.hh"
#include "phml/combine.h"
#include "phml/import.h"
#include "sbml.hh"
#include "utf8path.h"

#include "system.h"

using std::cerr;
using std::endl;
using std::memcpy;
using std::sprintf;
using std::strcmp;

namespace flint {
namespace phml {

namespace {

db::Driver *GetDriver(const boost::uuids::uuid &uuid)
{
	std::unique_ptr<char[]> db_file(new char[64]); // long enough
	std::string us = boost::uuids::to_string(uuid);
	sprintf(db_file.get(), "%s.db", us.c_str());
	return new db::Driver(db_file.get());
}

bool SaveFile(const boost::uuids::uuid &uuid, const char *xml_file)
{
	std::unique_ptr<db::Driver> driver(GetDriver(uuid));
	return SaveGivenFile(driver->db(), xml_file);
}

bool ParseFile(const boost::uuids::uuid &uuid)
{
	std::unique_ptr<db::Driver> driver(GetDriver(uuid));
	return flint::sbml::Parse(driver->db());
}

typedef std::vector<boost::uuids::uuid> UuidVector;

}

bool CombineAll(sqlite3 *db)
{
	UuidVector uv;
	sqlite3_stmt *stmt;
	int e = sqlite3_prepare_v2(db,
							   "SELECT m.module_id, i.type, i.ref FROM imports AS i LEFT JOIN modules AS m ON i.module_rowid = m.rowid",
							   -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		cerr << "failed to prepare statement: " << e << endl;
		return false;
	}

	for (e = sqlite3_step(stmt); e == SQLITE_ROW; e = sqlite3_step(stmt)) {
		const void *module_id = sqlite3_column_blob(stmt, 0);
		assert(module_id);
		boost::uuids::uuid uuid;
		memcpy(&uuid, module_id, uuid.size());
		const unsigned char *type = sqlite3_column_text(stmt, 1);
		const unsigned char *ref = sqlite3_column_text(stmt, 2);

		if (strcmp((const char *)type, "external") == 0) {
			if (!SaveFile(uuid, (const char *)ref)) return false;
		} else {
			if (!DumpImport(db, uuid)) return false;
			std::unique_ptr<char[]> xml_file(new char[64]);
			std::string us = boost::uuids::to_string(uuid);
			sprintf(xml_file.get(), "%s.xml", us.c_str());
			boost::filesystem::path xml_path = boost::filesystem::absolute(xml_file.get());
			std::unique_ptr<char[]> xml_utf8(GetUtf8FromPath(xml_path));
			if (!SaveFile(uuid, xml_utf8.get())) return false;
		}
		uv.push_back(uuid);
	}
	if (e != SQLITE_DONE) {
		cerr << "failed to step statement: " << e << endl;
		return false;
	}
	sqlite3_finalize(stmt);

	for (UuidVector::const_iterator it=uv.begin();it!=uv.end();++it) {
		if (!ParseFile(*it)) return false;
	}

	for (UuidVector::const_iterator it=uv.begin();it!=uv.end();++it) {
		if (!Combine(*it, db))
			return false;
	}

	return true;
}

}
}
