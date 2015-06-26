/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "method.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

#include "db/driver.hh"
#include "db/query.h"
#include "db/statement-driver.hh"
#include "method/helper.hh"

using std::cerr;
using std::endl;

using namespace boost::spirit;

namespace method {

namespace {

class Printer : public boost::static_visitor<> {
public:
	explicit Printer(std::ostream *os)
		: os_(os)
	{}

	void operator()(const Compound &c) const {
		os_->put('(');
		*os_ << c.keyword;
		std::vector<Expr>::const_iterator bit = c.children.begin();
		std::vector<Expr>::const_iterator eit = c.children.end();
		for (std::vector<Expr>::const_iterator it=bit;it!=eit;++it) {
			os_->put(' ');
			boost::apply_visitor(*this, *it);
		}
		os_->put(')');
	}

	void operator()(const std::string &s) const {
		*os_ << s;
	}

	void operator()(int i) const {
		*os_ << i;
	}

	void operator()(double d) const {
		*os_ << d;
	}

private:
	std::ostream *os_;
};

class VariantPrinter : public boost::static_visitor<> {
public:
	VariantPrinter(int k, int n, std::ostream *os)
		: k_(k)
		, n_(n)
		, os_(os)
	{}

	void operator()(const Compound &c) const {
		os_->put('(');
		*os_ << c.keyword;
		std::vector<Expr>::const_iterator bit = c.children.begin();
		std::vector<Expr>::const_iterator eit = c.children.end();
		for (std::vector<Expr>::const_iterator it=bit;it!=eit;++it) {
			os_->put(' ');
			boost::apply_visitor(*this, *it);
		}
		os_->put(')');
	}

	void operator()(const std::string &s) const {
		if (s[0] != '%') {
			*os_ << s;
			return;
		}
		if (s == "%time") {
			if (n_ == 1) {
				*os_ << "(plus %time @dt)";
			} else {
				*os_ << "(plus %time (divide @dt "
					 << n_
					 << "))";
			}
			return;
		}
		*os_ << s << '#' << k_;
	}

	void operator()(int i) const {
		*os_ << i;
	}

	void operator()(double d) const {
		*os_ << d;
	}

private:
	int k_;
	int n_;
	std::ostream *os_;
};

class Inserter : db::StatementDriver {
public:
	explicit Inserter(sqlite3 *db)
		: db::StatementDriver(db, "INSERT INTO asts VALUES (?, ?, ?)")
	{
	}

	bool PrintAndInsert(const char *uuid,
						const Expr &lhs,
						const Expr &rhs)
	{
		std::ostringstream oss;
		if (lhs.which() == kExprIsString) {
			const std::string &id(boost::get<std::string>(lhs));
			if (!Even(uuid, id, rhs, 2, 2)) return false;
			if (!Even(uuid, id, rhs, 4, 2)) return false;
			if (!Even(uuid, id, rhs, 6, 1)) return false;
			if (!Even(uuid, id, rhs, 0, 1)) return false;
			return true;
		} else {
			assert(lhs.which() == kExprIsCompound);
			const Compound &c(boost::get<Compound>(lhs));
			assert(c.children.size() == 2);
			const Expr &e(c.children.at(1));
			assert(e.which() == kExprIsString);
			const std::string &id(boost::get<std::string>(e));
			std::string name;
			std::string math;

			// #1: k1 = dt * f(t_n, y_n)
			oss.str("");
			oss << id << "#1";
			name = oss.str();
			oss.str("");
			oss << "(times @dt ";
			boost::apply_visitor(Printer(&oss), rhs);
			oss.put(')');
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #2: y1 = y_n + k1/2
			oss.str("");
			oss << id << "#2";
			name = oss.str();
			oss.str("");
			oss << "(plus " << id << " (divide " << id << "#1 2))";
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #3: k2 = dt * f(t_n + dt/2, y1)
			oss.str("");
			oss << id << "#3";
			name = oss.str();
			oss.str("");
			oss << "(times @dt ";
			boost::apply_visitor(VariantPrinter(2, 2, &oss), rhs);
			oss.put(')');
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #4: y2 = y_n + k2/2
			oss.str("");
			oss << id << "#4";
			name = oss.str();
			oss.str("");
			oss << "(plus " << id << " (divide " << id << "#3 2))";
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #5: k3 = dt * f(t_n + dt/2, y2)
			oss.str("");
			oss << id << "#5";
			name = oss.str();
			oss.str("");
			oss << "(times @dt ";
			boost::apply_visitor(VariantPrinter(4, 2, &oss), rhs);
			oss.put(')');
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #6: y3 = y_n + k3
			oss.str("");
			oss << id << "#6";
			name = oss.str();
			oss.str("");
			oss << "(plus " << id << ' ' << id << "#5)";
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #7: k4 = dt * f(t_n + dt, y3)
			oss.str("");
			oss << id << "#7";
			name = oss.str();
			oss.str("");
			oss << "(times @dt ";
			boost::apply_visitor(VariantPrinter(6, 1, &oss), rhs);
			oss.put(')');
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			// #0: y_(n+1) = y_n + (k1 + 2*k2 + 2*k3 + k4)/6
			oss.str("");
			oss << id << "#0";
			name = oss.str();
			oss.str("");
			oss << "(plus " << id << " (divide (plus " << id << "#1 (plus (times 2 " << id << "#3) (plus (times 2 " << id << "#5) " << id << "#7))) 6))";
			math = oss.str();
			if (!Insert(uuid, name.c_str(), math.c_str()))
				return false;

			return true;
		}
	}

private:
	bool Even(const char *uuid, const std::string &id, const Expr &rhs,
			  int k, int n) {
		std::ostringstream oss;
		oss << id << '#' << k;
		std::string name = oss.str();
		oss.str("");
		boost::apply_visitor(VariantPrinter(k, n, &oss), rhs);
		std::string math = oss.str();
		return Insert(uuid, name.c_str(), math.c_str());
	}

	bool Insert(const char *uuid, const char *name, const char *math) {
		int e;
		e = sqlite3_bind_text(stmt(), 1, uuid, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind uuid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt(), 2, name, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt(), 3, math, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(stmt());
		if (e != SQLITE_DONE) {
			cerr << "failed to step: " << e << endl;
			return false;
		}
		sqlite3_reset(stmt());
		return true;
	}
};

bool IsConditional(const Compound &x)
{
	return x.keyword == "case-set";
}

bool IsConditional(const Expr &x, Compound &y)
{
	if (x.which() != kExprIsCompound) return false;
	y = boost::get<Compound>(x);
	return IsConditional(y);
}

bool IsEquation(const Compound &x)
{
	return x.keyword == "eq" && x.children.size() >= 2;
}

bool IsEquation(const Expr &x, Compound &y)
{
	if (x.which() != kExprIsCompound) return false;
	y = boost::get<Compound>(x);
	return IsEquation(y);
}

void ReportInvalidLeafOfCaseSet(const std::string &uuid)
{
	cerr << "invalid formula found in <case-set>: " << uuid << endl;
}

bool TransformConditional(const Compound &c, Expr &lhs, Compound &rhs)
{
	rhs.keyword = "piecewise";
	rhs.children.clear();

	std::vector<Expr>::const_iterator bit = c.children.begin();
	std::vector<Expr>::const_iterator eit = c.children.end();
	for (std::vector<Expr>::const_iterator it=bit;it!=eit;++it) {
		assert(it->which() == kExprIsCompound);
		const Compound &cs = boost::get<Compound>(*it);
		assert(cs.keyword == "case");
		Compound r;
		Compound w;
		switch (cs.children.size()) {
		case 1:
			{
				r.keyword = "otherwise";
				r.children.resize(1);

				const Expr &v(cs.children[0]);
				if (IsConditional(v, w)) {
					Compound t;
					if (!TransformConditional(w, lhs, t)) return false;
					r.children[0] = t;
				} else if (IsEquation(v, w)) {
					lhs = w.children[0];
					r.children[0] = w.children[1];
				} else {
					return false;
				}

				rhs.children.push_back(r);
			}
			break;
		case 2:
			{
				r.keyword = "piece";
				r.children.resize(2);

				const Expr &v0(cs.children[0]);
				const Expr &v1(cs.children[1]);
				r.children[1] = v0;
				if (IsConditional(v1, w)) {
					Compound t;
					if (!TransformConditional(w, lhs, t)) return false;
					r.children[0] = t;
				} else if (IsEquation(v1, w)) {
					lhs = w.children[0];
					r.children[0] = w.children[1];
				} else {
					return false;
				}

				rhs.children.push_back(r);
			}
			break;
		default:
			assert(false);
			break;
		}
	}
	return true;
}

template<typename TLexer>
struct Lexer : lex::lexer<TLexer> {

	Lexer() {
		this->self.add_pattern
			("DIGIT", "[0-9]")
			("SIGN", "[-+]")
			("EXPONENT", "[eE]{SIGN}?{DIGIT}+")
			("FLOAT", "{SIGN}?({DIGIT}*\".\"{DIGIT}+{EXPONENT}?|{DIGIT}+{EXPONENT})")
			;

		case_set_ = "case-set";
		case_ = "case";
		condition_ = "condition";
		diff_ = "diff";
		eq_ = "eq";
		delay_ = "\"$\"Delay";
		delta_time_ = "\"$\"DeltaTime";

		real = "{FLOAT}";
		integer = "{SIGN}?{DIGIT}+";
		id = "\"%\"[a-zA-Z_][a-zA-Z_0-9:]*";
		keyword = "[$]?[a-zA-Z_][a-zA-Z_0-9]*";

		this->self = lex::token_def<>('\n') | '\r' | '(' | ')' | ' ';
		this->self += case_set_ | case_ | condition_ | diff_ | eq_ | delay_ | delta_time_;
		this->self += real | integer | id | keyword;
	}

	lex::token_def<std::string> case_set_, case_, condition_, eq_, diff_, delay_, delta_time_;
	lex::token_def<std::string> id, keyword;
	lex::token_def<int> integer;
	lex::token_def<double> real;
};

void RewriteDelayParam(Compound &x, const Expr &expr)
{
	Compound c;
	c.keyword = "minus";
	c.children.push_back("%time");
	c.children.push_back(expr);
	x.children.push_back(c);
}

template<typename TIterator>
struct Grammar : qi::grammar<TIterator, Compound()> {

	template<typename TTokenDef>
	Grammar(TTokenDef const &td)
	: Grammar::base_type(statement)
	{
		using boost::phoenix::at_c;
		using boost::phoenix::push_back;
		using boost::phoenix::val;

		statement = equation | conditional;

		equation = '(' >> td.eq_ [at_c<0>(_val) = _1]
					   >> ' ' >> (lexp | td.id) [push_back(at_c<1>(_val), _1)]
					   >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
					   >> ')';

		lexp = '(' >> td.diff_ [at_c<0>(_val) = _1]
				   >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
				   >> ' ' >> td.id [push_back(at_c<1>(_val), _1)]
				   >> ')';

		conditional = '(' >> td.case_set_
						  >> cseq
						  >> ')';

		cseq = +(' ' >> cexp [push_back(_val, _1)]);

		cexp = ('(' >> td.case_ [at_c<0>(_val) = _1]
				>> ' ' >> '(' >> td.condition_
				>> ' ' >> expr [push_back(at_c<1>(_val), _1)]
				>> ')'
				>> ' '
				>> statement [push_back(at_c<1>(_val), _1)]
				>> ')')
			| ('(' >> td.case_ [at_c<0>(_val) = _1]
			   >> ' '
			   >> statement [push_back(at_c<1>(_val), _1)]
			   >> ')');

		expr = delay_expr
			| delta_time_expr
			| eq_expr
			| general_expr
			| td.real
			| td.integer
			| td.id
			| td.keyword;

		delay_expr = '(' >> td.delay_ [at_c<0>(_val) = val("$lookback")]
						 >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
						 >> ' ' >> expr [bind(&RewriteDelayParam, _val, _1)]
						 >> ')';

		delta_time_expr = '(' >> td.delta_time_
							  >> ' ' >> td.id [bind(&RewriteDeltaTime, _val, _1)]
							  >> ')';

		eq_expr = '(' >> td.eq_
					  >> seq1
					  >> ')';

		general_expr = '(' >> td.keyword
						   >> seq0
						   >> ')';

		seq0 = *rest;

		seq1 = +rest;

		rest = ' ' >> expr [_val = _1];
	}

	qi::rule<TIterator, Expr()> expr, rest;
	qi::rule<TIterator, Compound()> statement, equation, lexp, conditional, cexp, delay_expr, delta_time_expr, eq_expr, general_expr;
	qi::rule<TIterator, std::vector<Expr>()> cseq, seq0, seq1;
};

/*
 * This class creates and keeps both tokens and grammar objects which
 * construction is expensive in terms of performance.
 */
class Parser {
public:
	typedef const char *base_iterator_type;
	typedef lex::lexertl::token<base_iterator_type> token_type;
	typedef lex::lexertl::lexer<token_type> lexer_type;
	typedef Lexer<lexer_type> RealLexer;
	typedef Grammar<RealLexer::iterator_type> RealGrammar;

	explicit Parser(sqlite3 *db)
		: tokens_()
		, grammar_(tokens_)
		, inserter_(db)
	{
	}

	int Parse(const char *uuid, const char *math) {
		base_iterator_type it = math;
		base_iterator_type eit = math + std::strlen(math);
		Compound statement;
		bool r = lex::tokenize_and_parse(it, eit, tokens_, grammar_, statement);
		if (!r || it != eit) {
			cerr << "failed to parse: " << it << endl;
			return 1;
		}
		return ProcessUuidAndStatement(uuid, statement);
	}

private:
	int ProcessUuidAndStatement(const char *uuid, Compound &statement)
	{
		if (IsConditional(statement)) {
			Expr lhs;
			Compound rhs;
			if (!TransformConditional(statement, lhs, rhs)) {
				ReportInvalidLeafOfCaseSet(uuid);
				return 1;
			}
			if (!inserter_.PrintAndInsert(uuid, lhs, rhs))
				return 1;
		} else if (IsEquation(statement)) {
			if (!inserter_.PrintAndInsert(uuid,
										  statement.children[0],
										  statement.children[1]))
				return 1;
		} else {
			assert(false);
			return 1;
		}
		return 0;
	}

	RealLexer tokens_;
	RealGrammar grammar_;
	Inserter inserter_;
};

int Process(void *data, int argc, char **argv, char **names)
{
	Parser *parser = static_cast<Parser *>(data);
	(void)names;
	assert(argc == 2);
	return parser->Parse(argv[0], argv[1]);
}

}

bool Rk4(sqlite3 *db, const char *input, sqlite3 *output)
{
	if (!SaveNol(8, output))
		return false;
	if (!CreateTable(output, "asts", "(uuid TEXT, name TEXT, math TEXT)"))
		return false;

	Parser parser(output);

	std::ostringstream oss;
	oss << "SELECT * FROM " << input;
	std::string query_i = oss.str();

	char *em;
	int e;
	e = sqlite3_exec(db, query_i.c_str(), Process, &parser, &em);
	if (e != SQLITE_OK) {
		cerr << "failed to exec: " << e
			 << ": " << em << endl;
		sqlite3_free(em);
		return false;
	}
	return true;
}

}
