/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "compiler.hh"

#include <cstdlib>
#include <cstdio>
#include <fstream>

#include "compiler/bcc.h"
#include "compiler/sort.h"
#include "compiler/tac.h"
#include "db/driver.hh"
#include "method.hh"

using std::cerr;
using std::endl;

namespace flint {
namespace compiler {

bool Compile(sqlite3 *db, const char *table, Method method, const char *output)
{
	db::Driver tmp("");
	switch (method) {
	case Method::kAssign:
		if (!method::Assign(db, table, tmp.db()))
			return false;
		break;
	case Method::kEuler:
		if (!method::Euler(db, table, tmp.db()))
			return false;
		break;
	case Method::kEvent:
		if (!method::Event(db, table, tmp.db()))
			return false;
		break;
	case Method::kRk4:
		if (!method::Rk4(db, table, tmp.db()))
			return false;
		break;
	}
	if (!compiler::sort::Sort(tmp.db()))
		return false;
	if (!compiler::tac::Tac(tmp.db()))
		return false;
	std::ofstream ofs(output, std::ios::binary);
	if (!ofs) {
		cerr << "failed to open " << output << endl;
		return false;
	}
	bool r = compiler::bcc::Bcc(tmp.db(), &ofs);
	ofs.close();
	return r;
}

}
}
