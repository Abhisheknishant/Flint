/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_COMPILER_HH_
#define FLINT_COMPILER_HH_

#include "sqlite3.h"

namespace flint {
namespace compiler {

enum class Method {
	kAssign,
	kEuler,
	kEvent,
	kRk4
};

/*
 * Note that db is for read only.
 */
bool Compile(sqlite3 *db, const char *table, Method method, const char *output);

/*
 * Note that db is for read only.
 */
bool GenerateBytecode(sqlite3 *db, const char *output);

}
}

#endif
