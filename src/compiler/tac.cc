/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "tac.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/variant/recursive_variant.hpp>

#include "db/query.h"
#include "db/statement-driver.h"

using std::cerr;
using std::endl;

using namespace boost::spirit;

namespace compiler {
namespace tac {

struct Compound;

enum {
	kExprIsCompound,
	kExprIsString,
	kExprIsInt,
	kExprIsDouble
};

typedef boost::variant<boost::recursive_wrapper<Compound>,
					   std::string,
					   int,
					   double
					   > Expr;

struct Compound {
	std::string keyword;
	std::deque<Expr> children;
};

}
}

BOOST_FUSION_ADAPT_STRUCT(compiler::tac::Compound,
						  (std::string, keyword)
						  (std::deque<compiler::tac::Expr>, children))

namespace compiler {
namespace tac {
namespace {

class Printer : public boost::static_visitor<> {
public:
	explicit Printer(std::ostream *os)
		: os_(os)
	{}

	void operator()(const Compound &c) const {
		*os_ << '(' << c.keyword.c_str();
		std::deque<Expr>::const_iterator bit = c.children.begin();
		std::deque<Expr>::const_iterator eit = c.children.end();
		for (std::deque<Expr>::const_iterator it=bit;it!=eit;++it) {
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

bool IsAnd(const Compound &c)
{
	return c.keyword == "and";
}

bool IsOr(const Compound &c)
{
	return c.keyword == "or";
}

bool IsAt(const Compound &c)
{
	return c.keyword == "$At";
}

bool IsLookback(const Compound &c)
{
	return c.keyword == "$lookback";
}

bool IsPiecewise(const Compound &c)
{
	return c.keyword == "piecewise";
}

bool IsPiece(const Compound &c)
{
	return c.keyword == "piece";
}

bool IsOtherwise(const Compound &c)
{
	return c.keyword == "otherwise";
}

bool IsTrial(const Compound &c)
{
	return c.keyword == "$trial";
}

bool IsOutcome(const Compound &c)
{
	return c.keyword == "$outcome";
}

struct Context {
	const char *uuid;
	const char *id;
	int avail_n;
	int avail_l;
	std::ostream *os;
};

bool EmitCode(int n, Expr &sexp, Context *context);

/*
 * The control should reach the end when the sexp is evaluated to be false.
 *
 * @param n the variable for the resulting boolean value
 * @param l the label to go when the sexp is evaluated to be true
 */
bool EmitCondition(int n, int l, Expr &sexp, Context *context)
{
	if (sexp.which() == kExprIsCompound) {
		Compound &comp(boost::get<Compound>(sexp));
		if (IsAnd(comp)) {
			int l1 = context->avail_l++;
			int l2 = context->avail_l++;
			if (!EmitCondition(n, l2, comp.children.at(0), context))
				return false;
			*context->os << "  jmp L" << l1 << endl;
			*context->os << " L" << l2 << ':' << endl;
			if (!EmitCondition(n, l, comp.children.at(1), context))
				return false;
			return *context->os << " L" << l1 << ':' << endl;
		} else if (IsOr(comp)) {
			return EmitCondition(n, l, comp.children.at(0), context)
				&& EmitCondition(n, l, comp.children.at(1), context);
		}
	}

	if (!EmitCode(n, sexp, context))
		return false;
	return *context->os << "  br $" << n << " L" << l << endl;
}

bool EmitAt(int n, std::deque<Expr> &children, Context *context)
{
	size_t len = children.size();
	if (len > 3) {
		cerr << "error: more than 3 arguments: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	if (len < 3) {
		cerr << "error: EmitAt: missing arguments: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	if (children.at(0).which() != kExprIsInt) {
		cerr << "error: invalid 1st argument of At: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	if (children.at(1).which() != kExprIsInt) {
		cerr << "error: invalid 2nd argument of At: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	int m = context->avail_n++;
	if (!EmitCode(m, children.at(2), context))
		return false;
	return *context->os << "  ld $"
						<< n
						<< ' '
						<< boost::get<int>(children.at(0))
						<< ' '
						<< boost::get<int>(children.at(1))
						<< " $"
						<< m
						<< endl;
}

bool EmitLookback(int n, std::deque<Expr> &children, Context *context)
{
	size_t len = children.size();
	if (len > 3) {
		cerr << "error: more than 2 arguments: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	if (len < 2) {
		cerr << "error: EmitLookback: missing arguments: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	if (children.at(0).which() != kExprIsString) {
		cerr << "error: invalid 1st argument of Delay/DeltaTime: " << context->uuid << ' ' << context->id << endl;
		return false;
	}
	int m = context->avail_n++;
	if (!EmitCode(m, children.at(1), context))
		return false;
	return *context->os << "  lb $"
						<< n
						<< ' '
						<< boost::get<std::string>(children.at(0)).c_str()
						<< " $"
						<< m
						<< endl;
}

bool EmitPiecewise(int n, std::deque<Expr> &children, Context *context)
{
	int l = context->avail_l++;
	std::vector<int> v1;
	bool otherwise = false;
	for (std::deque<Expr>::iterator it=children.begin();it!=children.end();++it) {
		assert(it->which() == kExprIsCompound);
		Compound &comp(boost::get<Compound>(*it));
		if (IsPiece(comp)) {
			int l1 = context->avail_l++;
			int m = context->avail_n++;
			if (!EmitCondition(m, l1, comp.children.at(1), context))
				return false;
			v1.push_back(l1);
		} else if (IsOtherwise(comp)) {
			otherwise = true;
			if (!EmitCode(n, comp.children.at(0), context))
				return false;
			*context->os << "  jmp L" << l << endl;
		}
	}
	if (!otherwise)
		*context->os << "  ret" << endl;
	std::vector<int>::const_iterator v1it = v1.begin();
	for (std::deque<Expr>::iterator it=children.begin();it!=children.end();++it) {
		assert(it->which() == kExprIsCompound);
		Compound &comp(boost::get<Compound>(*it));
		if (IsPiece(comp)) {
			*context->os << " L" << *v1it++ << ':' << endl;
			if (!EmitCode(n, comp.children.at(0), context))
				return false;
			*context->os << "  jmp L" << l << endl;
		} else if (IsOtherwise(comp)) {
			/* nothing to do */
		} else {
			assert(false);
		}
	}
	return *context->os << " L" << l << ':' << endl;
}

bool EmitTrial(int n, std::deque<Expr> &children, Context *context)
{
	int l = context->avail_l++;
	int p0 = context->avail_n++;
	int p1 = context->avail_n++;
	int p = context->avail_n++;
	*context->os << "  loadi $" << p0 << " 0" << endl
				 << "  loadi $" << p1 << " 1" << endl
				 << "  $" << p << " = ($uniform_variate $" << p0 << " $" << p1 << ')' << endl;
	std::vector<int> v1;
	for (std::deque<Expr>::iterator it=children.begin();it!=children.end();++it) {
		assert(it->which() == kExprIsCompound);
		Compound &comp(boost::get<Compound>(*it));
		if (IsOutcome(comp)) {
			int l1 = context->avail_l++;
			int m0 = context->avail_n++;
			*context->os << "  loadi $" << m0 << ' ';
			boost::apply_visitor(Printer(context->os), comp.children.at(1));
			*context->os << endl;
			int m1 = context->avail_n++;
			*context->os << "  $" << m1 << " = (leq $" << p << " $" << m0 << ')' << endl
						 << "  br $" << m1 << " L" << l1 << endl;
			v1.push_back(l1);
		} else {
			cerr << "error: unexpected child of $trial: " << context->uuid << ' ' << context->id << endl;
			return false;
		}
	}
	*context->os << "  ret" << endl;
	std::vector<int>::const_iterator v1it = v1.begin();
	for (std::deque<Expr>::iterator it=children.begin();it!=children.end();++it) {
		assert(it->which() == kExprIsCompound);
		Compound &comp(boost::get<Compound>(*it));
		if (IsOutcome(comp)) {
			*context->os << " L" << *v1it++ << ':' << endl;
			if (!EmitCode(n, comp.children.at(0), context))
				return false;
			*context->os << "  jmp L" << l << endl;
		} else {
			assert(false);
		}
	}
	return *context->os << " L" << l << ':' << endl;
}

bool EmitCode(int n, Expr &sexp, Context *context)
{
	int w = sexp.which();
	switch (w) {
	case kExprIsString:
		{
			const std::string &s(boost::get<std::string>(sexp));
			if ( (s[0] == '%' || s[0] == '@') && isalpha(s[1]) ) {
				*context->os << "  load $" << n << ' ' << s.c_str() << endl;
			} else {
				*context->os << "  loadi $" << n << ' ' << s.c_str() << endl;
			}
		}
		break;
	case kExprIsCompound:
		{
			Compound &comp(boost::get<Compound>(sexp));
			if (IsAt(comp)) {
				return EmitAt(n, comp.children, context);
			}
			if (IsLookback(comp)) {
				return EmitLookback(n, comp.children, context);
			}
			if (IsPiecewise(comp)) {
				return EmitPiecewise(n, comp.children, context);
			}
			if (IsTrial(comp)) {
				return EmitTrial(n, comp.children, context);
			}

			size_t len = comp.children.size();
			if (len > 2) {
				cerr << "error: more than 2 arguments for " << comp.keyword << ": "
					 << context->uuid << ' ' << context->id << endl;
				return false;
			}
			if (len < 1) {
				cerr << "error: missing arguments for " << comp.keyword << ": "
					 << context->uuid << ' ' << context->id << endl;
				return false;
			}
			for (std::deque<Expr>::iterator it=comp.children.begin();it!=comp.children.end();++it) {
				int m = context->avail_n++;
				if (!EmitCode(m, *it, context))
					return false;
				boost::scoped_array<char> buf(new char[20]);
				sprintf(buf.get(), "$%d", m);
				*it = buf.get();
			}
			*context->os << "  $" << n << " = ";
			boost::apply_visitor(Printer(context->os), sexp);
			*context->os << endl;
		}
		break;
	case kExprIsInt:
	case kExprIsDouble:
		{
			*context->os << "  loadi $" << n << ' ';
			boost::apply_visitor(Printer(context->os), sexp);
			*context->os << endl;
		}
		break;
	}
	return true;
}

bool EmitCode(const char *uuid, const char *id, Expr &sexp,
			  int *nod, std::ostream *os)
{
	boost::scoped_ptr<Context> context(new Context);
	context->uuid = uuid;
	context->id = id;
	context->avail_n = 1;
	context->avail_l = 0;
	context->os = os;
	if (!EmitCode(0, sexp, context.get()))
		return false;
	*os << "  store " << id << " $0" << endl;
	*nod = context->avail_n;
	return true;
}

void ReduceL(const char *op, Compound &c, std::deque<Expr> &children)
{
	size_t len = children.size();
	assert(len >= 2);
	c.keyword = op;
	if (len == 2) {
		c.children.swap(children);
		return;
	}
	c.children.push_back(children.back());
	children.pop_back();
	Compound c0;
	ReduceL(op, c0, children);
	c.children.push_front(c0);
}

void ReduceR(const char *op, Compound &c, std::deque<Expr> &children)
{
	size_t len = children.size();
	assert(len >= 2);
	c.keyword = op;
	if (len == 2) {
		c.children.swap(children);
		return;
	}
	c.children.push_front(children.front());
	children.pop_front();
	Compound c1;
	ReduceR(op, c1, children);
	c.children.push_back(c1);
}

void Negate(Compound &c, Expr &lexp)
{
	assert(lexp.which() == kExprIsCompound);
	Compound &lcomp(boost::get<Compound>(lexp));
	const std::string &s(lcomp.keyword);

	// in case of compound expression
	if (s == "and") {
		assert(lcomp.children.size() == 2);
		Compound c0;
		Compound c1;
		Negate(c0, lcomp.children.at(0));
		Negate(c1, lcomp.children.at(1));
		c.keyword = "or";
		c.children.push_back(c0);
		c.children.push_back(c1);
		return;
	}
	if (s == "or") {
		assert(lcomp.children.size() == 2);
		Compound c0;
		Compound c1;
		Negate(c0, lcomp.children.at(0));
		Negate(c1, lcomp.children.at(1));
		c.keyword = "and";
		c.children.push_back(c0);
		c.children.push_back(c1);
		return;
	}
	if (s == "not") {
		c = lcomp;
		return;
	}

	// in case of atomic expression
	if (s == "eq") {
		lcomp.keyword = "neq";
		c = lcomp;
		return;
	}
	if (s == "geq") {
		lcomp.keyword = "lt";
		c = lcomp;
		return;
	}
	if (s == "gt") {
		lcomp.keyword = "leq";
		c = lcomp;
		return;
	}
	if (s == "leq") {
		lcomp.keyword = "gt";
		c = lcomp;
		return;
	}
	if (s == "lt") {
		lcomp.keyword = "geq";
		c = lcomp;
		return;
	}
	if (s == "neq") {
		lcomp.keyword = "eq";
		c = lcomp;
		return;
	}

	assert(false); // FIXME
}

void Mean(Compound &c, std::deque<Expr> &children)
{
	size_t len = children.size();
	assert(len >= 2);
	c.keyword = "divide";
	Compound c1;
	ReduceR("plus", c1, children);
	c.children.push_back(c1);
	c.children.push_back((int)len);
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

		and_ = "and";
		eq_ = "eq";
		geq_ = "geq";
		gt_ = "gt";
		leq_ = "leq";
		log_ = "log";
		logbase_ = "logbase";
		lt_ = "lt";
		max_ = "max";
		mean_ = "mean";
		min_ = "min";
		neq_ = "neq";
		not_ = "not";
		or_ = "or";
		otherwise_ = "otherwise";
		piece_ = "piece";
		piecewise_ = "piecewise";
		plus_ = "plus";
		times_ = "times";
		xor_ = "xor";
		uniform_variate_ = "$uniform_variate";

		real = "{FLOAT}";
		integer = "{SIGN}?{DIGIT}+";
		id = "[%@][a-zA-Z_][a-zA-Z_0-9:#]*";
		keyword = "[$]?[a-zA-Z_][a-zA-Z_0-9]*";

		this->self = lex::token_def<>('\n') | '\r' | '(' | ')' | ' ';
		this->self += and_ | eq_ | geq_ | gt_ | leq_ | log_ | logbase_;
		this->self += lt_ | max_ | mean_ | min_ | neq_ | not_ | or_;
		this->self += otherwise_ | piece_ | piecewise_ | plus_ | times_ | xor_;
		this->self += uniform_variate_;
		this->self += real | integer | id | keyword;
	}

	lex::token_def<std::string> and_, eq_, geq_, gt_, leq_, log_, logbase_;
	lex::token_def<std::string> lt_, max_, mean_, min_, neq_, not_, or_;
	lex::token_def<std::string> otherwise_, piece_, piecewise_, plus_, times_, xor_;
	lex::token_def<std::string> uniform_variate_;
	lex::token_def<std::string> id, keyword;
	lex::token_def<int> integer;
	lex::token_def<double> real;
};

template<typename TIterator>
struct Grammar : qi::grammar<TIterator, Expr()> {

	template<typename TTokenDef>
	Grammar(TTokenDef const &td)
	: Grammar::base_type(expr)
	{
		using boost::phoenix::at_c;
		using boost::phoenix::push_back;
		using boost::phoenix::val;

		expr %= td.real
			| td.integer
			| td.id
			| td.keyword
			| '(' >> compound >> ')';

		compound = td.log_ [at_c<0>(_val) = _1] >> ' '
												>> '(' >> td.logbase_ >> ' ' >> expr [push_back(at_c<1>(_val), _1)] >> ')'
												>> ' ' >> expr [push_back(at_c<1>(_val), _1)]
			| td.log_ [at_c<0>(_val) = val("log10")] >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
			| td.piecewise_ [at_c<0>(_val) = _1] >> pseq1 [at_c<1>(_val) = _1]
			| td.max_ >> seq1 [bind(&ReduceR, val("max"), _val, _1)]
			| td.mean_ >> seq1 [bind(&Mean, _val, _1)]
			| td.min_ >> seq1 [bind(&ReduceR, val("min"), _val, _1)]
			| td.plus_ >> seq1 [bind(&ReduceR, val("plus"), _val, _1)]
			| td.times_ >> seq1 [bind(&ReduceR, val("times"), _val, _1)]
			| td.uniform_variate_ [at_c<0>(_val) = _1]
						   >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
						   >> ' ' >> expr [push_back(at_c<1>(_val), _1)]
						   >> ' ' >> expr
						   >> ' ' >> expr
			| td.keyword [at_c<0>(_val) = _1] >> seq0 [at_c<1>(_val) = _1];

		pseq1 = +prest;

		prest = ' ' >> pexp [_val = _1];

		pexp = '(' >> pcomp [_val = _1] >> ')';

		pcomp = td.piece_ [at_c<0>(_val) = _1] >> ' ' >> expr [push_back(at_c<1>(_val), _1)] >> ' ' >> lexp [push_back(at_c<1>(_val), _1)]
			| td.otherwise_ [at_c<0>(_val) = _1] >> ' ' >> expr [push_back(at_c<1>(_val), _1)];

		lexp = '(' >> lcomp [_val = _1] >> ')';

		lcomp = td.and_ >> lseq1 [bind(&ReduceL, val("and"), _val, _1)]
			| td.or_ >> lseq1 [bind(&ReduceL, val("or"), _val, _1)]
			| td.xor_ >> lseq1 [bind(&ReduceL, val("neq"), _val, _1)] // logical XOR can be considered as NEQ
			| td.not_ >> ' ' >> lexp [bind(&Negate, _val, _1)]
			| (td.eq_ | td.geq_ | td.gt_ | td.leq_ | td.lt_ | td.neq_) [at_c<0>(_val) = _1] >> ' ' >> expr [push_back(at_c<1>(_val), _1)] >> ' ' >> expr [push_back(at_c<1>(_val), _1)];

		lseq1 = +lrest;

		lrest = ' ' >> lexp [_val = _1];

		seq0 = *rest;

		seq1 = +rest;

		rest = ' ' >> expr [_val = _1];
	}

	qi::rule<TIterator, Expr()> expr, pexp, lexp, rest, prest, lrest;
	qi::rule<TIterator, Compound()> compound, pcomp, lcomp;
	qi::rule<TIterator, std::deque<Expr>()> seq0, seq1, pseq1, lseq1;
};

class Inserter : db::StatementDriver {
public:
	Inserter(sqlite3 *db)
		: db::StatementDriver(db, "INSERT INTO tacs VALUES (?, ?, ?, ?)")
	{
	}

	bool Insert(const char *uuid, const char *name, int nod, const char *body) {
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
		e = sqlite3_bind_int(stmt(), 3, nod);
		if (e != SQLITE_OK) {
			cerr << "failed to bind nod: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt(), 4, body, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind body: " << e << endl;
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

int Process(void *data, int argc, char **argv, char **names)
{
	typedef const char *base_iterator_type;
	typedef lex::lexertl::token<base_iterator_type> token_type;
	typedef lex::lexertl::lexer<token_type> lexer_type;
	typedef Lexer<lexer_type> RealLexer;
	typedef Grammar<RealLexer::iterator_type> RealGrammar;

	static const RealLexer tokens;
	static const RealGrammar grammar(tokens);

	(void)names;
	Inserter *inserter = (Inserter *)data;
	assert(argc == 3);
	const char *uuid = argv[0];
	const char *name = argv[1];
	const char *math = argv[2];
	base_iterator_type it = math;
	base_iterator_type eit = math + std::strlen(math);
	Expr expr;
	bool r = lex::tokenize_and_parse(it, eit, tokens, grammar, expr);
	if (!r || it != eit) {
		cerr << "failed to parse: " << it << endl;
		return 1;
	}
	std::ostringstream oss;
	int nod;
	if (!EmitCode(uuid, name, expr, &nod, &oss))
		return 1;
	std::string body = oss.str();
	return (inserter->Insert(uuid, name, nod, body.c_str())) ? 0 : 1;
}

}

bool Tac(sqlite3 *db)
{
	if (!BeginTransaction(db))
		return false;
	if (!CreateTable(db, "tacs", "(uuid TEXT, name TEXT, nod INTEGER, body TEXT)"))
		return false;

	Inserter inserter(db);
	char *em;
	int e;
	e = sqlite3_exec(db, "SELECT * FROM sorts", Process, &inserter, &em);
	if (e != SQLITE_OK) {
		cerr << "failed to enumerate sorts: " << e
			 << ": " << em << endl;
		sqlite3_free(em);
		return false;
	}
	return CommitTransaction(db);
}

}
}