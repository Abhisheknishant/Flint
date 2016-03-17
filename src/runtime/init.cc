/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "runtime.h"

#include <cstdio>
#include <iostream>
#include <memory>

#include "runtime/evaluator.h"

using std::cerr;
using std::endl;

namespace flint {
namespace runtime {

bool Init(sqlite3 *db,
		  int seed,
		  const char *layout_file, const char *bc_file, const char *output_file)
{
	std::unique_ptr<Evaluator> e(new Evaluator);
	if (!e->Load(layout_file))
		return false;
	if (!e->layout().ContainsDependentVariable()) {
		cerr << "no dependent variables found" << endl;
		return false;
	}
	return e->Evaluate(db, seed, bc_file, output_file);
}

}
}
