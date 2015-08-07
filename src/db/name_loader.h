/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_DB_NAME_LOADER_H_
#define FLINT_DB_NAME_LOADER_H_

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <boost/uuid/uuid_io.hpp>

#include "statement-driver.hh"

namespace db {

class NameLoader : StatementDriver {
public:
	// Note that db is for read only.
	explicit NameLoader(sqlite3 *db)
		: StatementDriver(db, "SELECT * FROM names")
	{
	}

	template<typename THandler>
	bool Load(THandler *handler) {
		int e;
		for (e = sqlite3_step(stmt()); e == SQLITE_ROW; e = sqlite3_step(stmt())) {
			const void *space_id = sqlite3_column_blob(stmt(), 0);
			const unsigned char *type = sqlite3_column_text(stmt(), 1);
			int id = sqlite3_column_int(stmt(), 2);
			const unsigned char *name = sqlite3_column_text(stmt(), 3);
			const unsigned char *unit = sqlite3_column_text(stmt(), 4);
			double capacity = sqlite3_column_double(stmt(), 5);

			assert(space_id);
			boost::uuids::uuid u;
			std::memcpy(&u, space_id, u.size());
			if (!name) {
				std::cerr << "name for "
						  << u
						  << ":"
						  << id
						  << " is not found"
						  << std::endl;
				return false;
			}
			if (!unit) {
				std::cerr << "unit for "
						  << u
						  << ":"
						  << name
						  << " is not found"
						  << std::endl;
				return false;
			}
			if (!handler->Handle(u, (char)type[0], id, (const char *)name, (const char *)unit, capacity))
				return false;
		}
		if (e != SQLITE_DONE) {
			std::cerr << "failed to step statement: " << e << std::endl;
			return false;
		}
		sqlite3_reset(stmt());
		return true;
	}
};

} // namespace db

#endif
