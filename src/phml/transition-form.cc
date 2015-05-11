/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "transition-form.h"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <boost/ptr_container/ptr_map.hpp>

using std::cerr;
using std::endl;

namespace {

const char kQuerySelect[] = \
	"SELECT a.*, p.name FROM arcs AS a"
	" LEFT JOIN pqs AS p ON a.pq_rowid = p.rowid";

const char kQueryExtras[] = "INSERT INTO extras VALUES (?, 'before', ?)";

const char kQueryImpls[] = "INSERT INTO impls VALUES (?, ?)";

class Arc {
public:
	Arc(int head_node_id, const char *type, const char *math)
		: head_node_id_(head_node_id),
		  type_(type),
		  math_(math)
	{
	}

	int head_node_id() const {return head_node_id_;}
	const std::string &type() const {return type_;}
	const std::string &math() const {return math_;}

private:
	int head_node_id_;
	std::string type_;
	std::string math_;
};

typedef std::map<sqlite3_int64, std::string> NameMap;
typedef std::multimap<int, Arc> ArcMultimap;
typedef boost::ptr_map<sqlite3_int64, ArcMultimap> PqArcMap;

bool Insert(sqlite3_stmt *stmt, sqlite3_int64 pq_rowid, const std::string &math)
{
	int e;
	e = sqlite3_bind_int64(stmt, 1, pq_rowid);
	if (e != SQLITE_OK) {
		cerr << "failed to bind pq_rowid: " << e << endl;
		return false;
	}
	e = sqlite3_bind_text(stmt, 2, math.c_str(), -1, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		cerr << "failed to bind math: " << e << endl;
		return false;
	}
	e = sqlite3_step(stmt);
	if (e != SQLITE_DONE) {
		cerr << "failed to step statement: " << e << endl;
		return false;
	}
	sqlite3_reset(stmt);
	return true;
}

} // namespace

namespace phml {

TransitionForm::TransitionForm(sqlite3 *db)
	: stmt_select_(NULL),
	  stmt_extras_(NULL),
	  stmt_impls_(NULL)
{
	int e;
	e = sqlite3_prepare_v2(db, kQuerySelect, -1, &stmt_select_, NULL);
	if (e != SQLITE_OK) {
		cerr << "failed to prepare statement: " << kQuerySelect << ": " << e << endl;
		exit(EXIT_FAILURE);
	}
	e = sqlite3_prepare_v2(db, kQueryExtras, -1, &stmt_extras_, NULL);
	if (e != SQLITE_OK) {
		cerr << "failed to prepare statement: " << kQueryExtras << ": " << e << endl;
		exit(EXIT_FAILURE);
	}
	e = sqlite3_prepare_v2(db, kQueryImpls, -1, &stmt_impls_, NULL);
	if (e != SQLITE_OK) {
		cerr << "failed to prepare statement: " << kQueryImpls << ": " << e << endl;
		exit(EXIT_FAILURE);
	}
}

TransitionForm::~TransitionForm()
{
	sqlite3_finalize(stmt_select_);
	sqlite3_finalize(stmt_extras_);
	sqlite3_finalize(stmt_impls_);
}

bool TransitionForm::operator()()
{
	NameMap nm;
	PqArcMap pam;
	int e;
	for (e = sqlite3_step(stmt_select_); e == SQLITE_ROW; e = sqlite3_step(stmt_select_)) {
		sqlite3_int64 pq_rowid = sqlite3_column_int64(stmt_select_, 0);
		int tail_node_id = sqlite3_column_int(stmt_select_, 1);
		int head_node_id = sqlite3_column_int(stmt_select_, 2);
		const unsigned char *type = sqlite3_column_text(stmt_select_, 3);
		const unsigned char *math = sqlite3_column_text(stmt_select_, 4);
		const unsigned char *pq_name = sqlite3_column_text(stmt_select_, 5);
		nm.insert(std::make_pair(pq_rowid, std::string((const char *)pq_name)));
		pam[pq_rowid].insert(std::make_pair(tail_node_id, Arc(head_node_id,
															  (const char *)type,
															  (const char *)math)));
	}
	if (e != SQLITE_DONE) {
		cerr << "failed to step statement: " << kQuerySelect << ": " << e << endl;
		return false;
	}
	for (NameMap::const_iterator nit=nm.begin();nit!=nm.end();++nit) {
		sqlite3_int64 pq_rowid(nit->first);
		const std::string &pq_name(nit->second);
		const ArcMultimap &am(pam.at(pq_rowid));

		std::ostringstream se;
		se << " (eq %"
		   << pq_name
		   << " (piecewise";

		ArcMultimap::const_iterator ait = am.begin();
		ArcMultimap::const_iterator bit;
		do {
			int tail_node_id = ait->first;
			const std::string &type(ait->second.type());
			se << " (piece";
			if (type == "condition") {
				se << " (piecewise";
			} else {
				assert(type == "probability");
				se << " ($trial";
			}
			double p = 0;
			for (bit=am.upper_bound(tail_node_id);ait!=bit;++ait) {
				const Arc &arc(ait->second);
				if (type != arc.type()) {
					cerr << "incompatible types among <transition>s of the same PQ" << endl;
					return false;
				}
				if (type == "condition") {
					se << " (piece ";
				} else { // probability
					se << " ($outcome ";
				}
				se << arc.head_node_id();
				if (type == "condition") {
					se << arc.math();
				} else {
					p += atof(arc.math().c_str());
					se << ' ' << p;
				}
				se << ")";
			}
			se << ") (eq %"
			   << pq_name
			   << " "
			   << tail_node_id
			   << "))";
		} while (bit != am.end());
		se << "))";
		if (!Insert(stmt_extras_, pq_rowid, se.str())) return false;

		std::ostringstream si;
		si << " (eq (diff (bvar %time) %" << nit->second << ") 0)";
		if (!Insert(stmt_impls_, pq_rowid, si.str())) return false;
	}
	return true;
}

} // namespace phml