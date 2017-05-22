/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_FILTER_H_
#define FLINT_FILTER_H_

#include <cstdio>
#include "sqlite3.h"

namespace flint {
namespace filter {

/*
 * Note that db is for read only.
 * Return true in case of success, false otherwise.
 */
bool Create(sqlite3 *db, const char *spec_file, const char *layout_file, const char *output_file);

/*
 * Return true in case of success, false otherwise.
 */
bool Track(const char *filter_file, const char *output_file);

/*
 * Return true in case of success, false otherwise.
 */
bool Isdh(const char *filter_file, const char *output_file);

}
}

#endif
