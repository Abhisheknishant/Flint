/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_PHML_GRAPH_MATH_REWRITER_H_
#define FLINT_PHML_GRAPH_MATH_REWRITER_H_

#include <boost/noncopyable.hpp>

#include "sqlite3.h"

namespace phml {

class GraphMathRewriter : boost::noncopyable {
public:
	GraphMathRewriter(sqlite3 *db,
					  const char *query_select,
					  const char *query_update);

	~GraphMathRewriter();

	bool Rewrite();

	bool FindNode(sqlite3_int64 pq_rowid, const char *node_name, int *node_id);

private:
	bool Process(sqlite3_int64 rowid, sqlite3_int64 pq_rowid, const char *math);

	bool Update(sqlite3_int64 rowid, const char *math);

	const char *query_select_;
	const char *query_update_;
	sqlite3_stmt *stmt_select_;
	sqlite3_stmt *stmt_node_;
	sqlite3_stmt *stmt_update_;
};

} // namespace phml

#endif
