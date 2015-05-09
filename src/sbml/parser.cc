/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "parser.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

#include <sbmlsolver/odeModel.h>
#include <sbmlsolver/solverError.h>

#include "db/driver.h"
#include "db/query.h"
#include "modelpath.h"

using std::cerr;
using std::endl;
using std::printf;
using std::putchar;
using std::string;
using std::strlen;

namespace {

bool PrintChildrenToOstream(const ASTNode_t *node, std::ostream *os);
bool PrintPiecesToOstream(const ASTNode_t *node, std::ostream *os);

bool PrintToOstream(const ASTNode_t *node, std::ostream *os)
{
	switch (ASTNode_getType(node)) {
	case AST_PLUS:
		*os << " (plus";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_MINUS:
		*os << " (minus";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_TIMES:
		*os << " (times";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_DIVIDE:
		*os << " (divide";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_POWER:
		*os << " (power";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_INTEGER:
		*os << ' ' << ASTNode_getInteger(node);
		break;
	case AST_REAL:
	case AST_REAL_E:
	case AST_RATIONAL:
		*os << ' ' << ASTNode_getReal(node);
		break;
	case AST_NAME:
		*os << " %sbml:" << ASTNode_getName(node);
		break;
	case AST_NAME_TIME:
		*os << " %time";
		break;
	case AST_FUNCTION:
		cerr << "not yet support user-defined functions" << endl;
		return false;
	case AST_FUNCTION_ABS:
		*os << " (abs";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCOS:
		*os << " (arccos";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCOSH:
		*os << " (arccosh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCOT:
		*os << " (arccot";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCOTH:
		*os << " (arccoth";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCSC:
		*os << " (arccsc";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCCSCH:
		*os << " (arccsch";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCSEC:
		*os << " (arcsec";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCSECH:
		*os << " (arcsech";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCSIN:
		*os << " (arcsin";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCSINH:
		*os << " (arcsinh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCTAN:
		*os << " (arctan";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ARCTANH:
		*os << " (arctanh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_CEILING:
		*os << " (ceil";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_COS:
		*os << " (cos";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_COSH:
		*os << " (cosh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_COT:
		*os << " (cot";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_COTH:
		*os << " (coth";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_CSC:
		*os << " (csc";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_CSCH:
		*os << " (csch";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_DELAY:
		cerr << "not yet support DELAY" << endl;
		return false;
	case AST_FUNCTION_EXP:
		*os << " (exp";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_FACTORIAL:
		*os << " (factorial";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_FLOOR:
		*os << " (floor";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_LN:
		*os << " (ln";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_LOG:
		*os << " (log";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_PIECEWISE:
		*os << " (piecewise";
		if (!PrintPiecesToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_POWER:
		*os << " (power";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_ROOT:
		*os << " (root";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_SEC:
		*os << " (sec";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_SECH:
		*os << " (sech";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_SIN:
		*os << " (sin";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_SINH:
		*os << " (sinh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_TAN:
		*os << " (tan";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_FUNCTION_TANH:
		*os << " (tanh";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_EQ:
		*os << " (eq";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_GEQ:
		*os << " (geq";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_GT:
		*os << " (gt";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_LEQ:
		*os << " (leq";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_LT:
		*os << " (lt";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_RELATIONAL_NEQ:
		*os << " (neq";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_LOGICAL_AND:
		*os << " (and";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_LOGICAL_NOT:
		*os << " (not";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_LOGICAL_OR:
		*os << " (or";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	case AST_LOGICAL_XOR:
		*os << " (xor";
		if (!PrintChildrenToOstream(node, os)) return false;
		os->put(')');
		break;
	default:
		if (ASTNode_isBoolean(node)) {
			cerr << "not yet support Boolean" << endl;
			return false;
		} else if (ASTNode_isConstant(node)) { // true, false, pi, exponentiale
			switch (ASTNode_getType(node)) {
			case AST_CONSTANT_E:
				*os << " exponentiale";
				break;
			case AST_CONSTANT_FALSE:
				*os << " false";
				break;
			case AST_CONSTANT_PI:
				*os << " pi";
				break;
			case AST_CONSTANT_TRUE:
				*os << " true";
				break;
			default:
				cerr << "unknown Constant: " << ASTNode_getType(node) << endl;
				return false;
			}
		} else if (ASTNode_isInfinity(node)) {
			cerr << "not yet support Infinity" << endl;
			return false;
		} else if (ASTNode_isLambda(node)) {
			cerr << "not yet support Lambda" << endl;
			return false;
		} else if (ASTNode_isNaN(node)) {
			cerr << "not yet support NaN" << endl;
			return false;
		} else if (ASTNode_isNegInfinity(node)) {
			cerr << "not yet support NegInfinity" << endl;
			return false;
		} else if (ASTNode_isPiecewise(node)) {
			assert(false); // this shoud not happen
		} else {
			cerr << "unknown node" << endl;
			return false;
		}
	}
	return true;
}

bool PrintChildrenToOstream(const ASTNode_t *node, std::ostream *os)
{
	int num = ASTNode_getNumChildren(node);
	for (int i=0; i<num; i++) {
		if (!PrintToOstream(ASTNode_getChild(node, i), os)) return false;
	}
	return true;
}

bool PrintPiecesToOstream(const ASTNode_t *node, std::ostream *os)
{
	int num = ASTNode_getNumChildren(node);
	if (num <= 0) {
		cerr << "neither <piece> nor <otherwise> in <piecewise>" << endl;
		return false;
	}
	int i = 0;
	while (i < num-1) {
		*os << " (piece";
		PrintToOstream(ASTNode_getChild(node, i), os);
		PrintToOstream(ASTNode_getChild(node, i+1), os);
		os->put(')');
		i += 2;
	}
	*os << " (otherwise";
	PrintToOstream(ASTNode_getChild(node, num-1), os);
	os->put(')');
	return true;
}

class Analysis : public db::Driver {
public:
	Analysis(const char *db_file, odeModel_t *model)
		: db::Driver(db_file)
		, model_(model)
	{
		assert(model);

	}

	~Analysis() {
		if (stmt_a_) sqlite3_finalize(stmt_a_);
		if (stmt_c_) sqlite3_finalize(stmt_c_);
		if (stmt_o_) sqlite3_finalize(stmt_o_);
		ODEModel_free(model_);
	}

	bool Run() {
		if ( !CreateTable(db(), "assignments", "(name TEXT, math TEXT)") ||
			 !CreateTable(db(), "constants", "(name TEXT, value REAL)") ||
			 !CreateTable(db(), "odes", "(name TEXT, initial_value REAL, math TEXT)") )
			return false;

		int e;
		e = sqlite3_prepare_v2(db(), "INSERT INTO assignments VALUES (?, ?)",
							   -1, &stmt_a_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			return false;
		}
		e = sqlite3_prepare_v2(db(), "INSERT INTO constants VALUES (?, ?)",
							   -1, &stmt_c_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			return false;
		}
		e = sqlite3_prepare_v2(db(), "INSERT INTO odes VALUES (?, ?, ?)",
							   -1, &stmt_o_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			return false;
		}

		if (!BeginTransaction(db())) return false;

		boost::scoped_array<char> sbml_name;
		int neq = ODEModel_getNeq(model_);
		int nass = ODEModel_getNumAssignments(model_);
		int nconst = ODEModel_getNumConstants(model_);
		for (int i=0; i<neq; i++) {
			variableIndex_t *vi = ODEModel_getOdeVariableIndex(model_, i);
			if (!vi) break;
			const ASTNode_t *node = ODEModel_getOde(model_, vi);
			if (!node) {
				VariableIndex_free(vi);
				break;
			}
			printf("o sbml:%s", VariableIndex_getName(vi, model_));
			printf(" %g", model_->values[i]);
			if (!Print(node)) return false;
			putchar('\n');

			const char *name = VariableIndex_getName(vi, model_);
			sbml_name.reset(new char[6+strlen(name)]);
			sprintf(sbml_name.get(), "sbml:%s", name);
			VariableIndex_free(vi);
			std::ostringstream oss;
			if (!PrintToOstream(node, &oss)) return false;
			std::string math = oss.str();
			if (!InsertOde(sbml_name.get(), model_->values[i], math.c_str()))
				return false;
		}
		for (int i=0; i<nass; i++) {
			variableIndex_t *vi = ODEModel_getAssignedVariableIndex(model_, i);
			if (!vi) break;
			const ASTNode_t *node = ODEModel_getAssignment(model_, vi);
			if (!node) {
				VariableIndex_free(vi);
				break;
			}
			printf("a sbml:%s", VariableIndex_getName(vi, model_));
			if (!Print(node)) return false;
			putchar('\n');

			const char *name = VariableIndex_getName(vi, model_);
			sbml_name.reset(new char[6+strlen(name)]);
			sprintf(sbml_name.get(), "sbml:%s", name);
			VariableIndex_free(vi);
			std::ostringstream oss;
			if (!PrintToOstream(node, &oss)) return false;
			std::string math = oss.str();
			if (!InsertAssignment(sbml_name.get(), math.c_str()))
				return false;
		}
		for (int i=0; i<nconst; i++) {
			variableIndex_t *vi = ODEModel_getConstantIndex(model_, i);
			if (!vi) break;
			printf("c sbml:%s", VariableIndex_getName(vi, model_));
			printf(" %g\n", model_->values[neq+nass+i]);

			const char *name = VariableIndex_getName(vi, model_);
			sbml_name.reset(new char[6+strlen(name)]);
			sprintf(sbml_name.get(), "sbml:%s", name);
			VariableIndex_free(vi);
			if (!InsertConstant(sbml_name.get(), model_->values[neq+nass+i]))
				return false;
		}
		return CommitTransaction(db());
	}

private:
	bool InsertAssignment(const char *name, const char *math)
	{
		int e;
		e = sqlite3_bind_text(stmt_a_, 1, name, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt_a_, 2, math, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(stmt_a_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(stmt_a_);
		return true;
	}

	bool InsertConstant(const char *name, double value)
	{
		int e;
		e = sqlite3_bind_text(stmt_c_, 1, name, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_double(stmt_c_, 2, value);
		if (e != SQLITE_OK) {
			cerr << "failed to bind value: " << e << endl;
			return false;
		}
		e = sqlite3_step(stmt_c_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(stmt_c_);
		return true;
	}

	bool InsertOde(const char *name, double initial_value, const char *math)
	{
		int e;
		e = sqlite3_bind_text(stmt_o_, 1, name, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_double(stmt_o_, 2, initial_value);
		if (e != SQLITE_OK) {
			cerr << "failed to bind initial_value: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt_o_, 3, math, -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(stmt_o_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(stmt_o_);
		return true;
	}

	bool Print(const ASTNode_t *node) const {
		switch (ASTNode_getType(node)) {
		case AST_PLUS:
			printf(" (plus");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_MINUS:
			printf(" (minus");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_TIMES:
			printf(" (times");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_DIVIDE:
			printf(" (divide");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_POWER:
			printf(" (power");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_INTEGER:
			printf(" %ld", ASTNode_getInteger(node));
			break;
		case AST_REAL:
		case AST_REAL_E:
		case AST_RATIONAL:
			printf(" %g", ASTNode_getReal(node));
			break;
		case AST_NAME:
			printf(" %%sbml:%s", ASTNode_getName(node));
			break;
		case AST_NAME_TIME:
			printf(" %%time");
			break;
		case AST_FUNCTION:
			cerr << "not yet support user-defined functions" << endl;
			return false;
		case AST_FUNCTION_ABS:
			printf(" (abs");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCOS:
			printf(" (arccos");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCOSH:
			printf(" (arccosh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCOT:
			printf(" (arccot");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCOTH:
			printf(" (arccoth");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCSC:
			printf(" (arccsc");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCCSCH:
			printf(" (arccsch");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCSEC:
			printf(" (arcsec");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCSECH:
			printf(" (arcsech");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCSIN:
			printf(" (arcsin");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCSINH:
			printf(" (arcsinh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCTAN:
			printf(" (arctan");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ARCTANH:
			printf(" (arctanh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_CEILING:
			printf(" (ceil");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_COS:
			printf(" (cos");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_COSH:
			printf(" (cosh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_COT:
			printf(" (cot");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_COTH:
			printf(" (coth");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_CSC:
			printf(" (csc");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_CSCH:
			printf(" (csch");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_DELAY:
			cerr << "not yet support DELAY" << endl;
			return false;
		case AST_FUNCTION_EXP:
			printf(" (exp");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_FACTORIAL:
			printf(" (factorial");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_FLOOR:
			printf(" (floor");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_LN:
			printf(" (ln");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_LOG:
			printf(" (log");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_PIECEWISE:
			printf(" (piecewise");
			if (!PrintPieces(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_POWER:
			printf(" (power");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_ROOT:
			printf(" (root");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_SEC:
			printf(" (sec");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_SECH:
			printf(" (sech");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_SIN:
			printf(" (sin");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_SINH:
			printf(" (sinh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_TAN:
			printf(" (tan");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_FUNCTION_TANH:
			printf(" (tanh");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_EQ:
			printf(" (eq");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_GEQ:
			printf(" (geq");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_GT:
			printf(" (gt");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_LEQ:
			printf(" (leq");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_LT:
			printf(" (lt");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_RELATIONAL_NEQ:
			printf(" (neq");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_LOGICAL_AND:
			printf(" (and");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_LOGICAL_NOT:
			printf(" (not");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_LOGICAL_OR:
			printf(" (or");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		case AST_LOGICAL_XOR:
			printf(" (xor");
			if (!PrintChildren(node)) return false;
			putchar(')');
			break;
		default:
			if (ASTNode_isBoolean(node)) {
				cerr << "not yet support Boolean" << endl;
				return false;
			} else if (ASTNode_isConstant(node)) { // true, false, pi, exponentiale
				switch (ASTNode_getType(node)) {
				case AST_CONSTANT_E:
					printf(" exponentiale");
					break;
				case AST_CONSTANT_FALSE:
					printf(" false");
					break;
				case AST_CONSTANT_PI:
					printf(" pi");
					break;
				case AST_CONSTANT_TRUE:
					printf(" true");
					break;
				default:
					cerr << "unknown Constant: " << ASTNode_getType(node) << endl;
					return false;
				}
			} else if (ASTNode_isInfinity(node)) {
				cerr << "not yet support Infinity" << endl;
				return false;
			} else if (ASTNode_isLambda(node)) {
				cerr << "not yet support Lambda" << endl;
				return false;
			} else if (ASTNode_isNaN(node)) {
				cerr << "not yet support NaN" << endl;
				return false;
			} else if (ASTNode_isNegInfinity(node)) {
				cerr << "not yet support NegInfinity" << endl;
				return false;
			} else if (ASTNode_isPiecewise(node)) {
				assert(false); // this shoud not happen
			} else {
				cerr << "unknown node" << endl;
				return false;
			}
		}
		return true;
	}

	bool PrintChildren(const ASTNode_t *node) const {
		int num = ASTNode_getNumChildren(node);
		for (int i=0; i<num; i++) {
			if (!Print(ASTNode_getChild(node, i))) return false;
		}
		return true;
	}

	bool PrintPieces(const ASTNode_t *node) const {
		int num = ASTNode_getNumChildren(node);
		if (num <= 0) {
			cerr << "neither <piece> nor <otherwise> in <piecewise>" << endl;
			return false;
		}
		int i = 0;
		while (i < num-1) {
			printf(" (piece");
			Print(ASTNode_getChild(node, i));
			Print(ASTNode_getChild(node, i+1));
			putchar(')');
			i += 2;
		}
		printf(" (otherwise");
		Print(ASTNode_getChild(node, num-1));
		putchar(')');
		return true;
	}

	odeModel_t *model_;
	sqlite3_stmt *stmt_a_;
	sqlite3_stmt *stmt_c_;
	sqlite3_stmt *stmt_o_;
};

} // namespace

bool ParseSbml(const char *db_file)
{
	boost::scoped_array<char> model_file(GetModelFilename(db_file));
	odeModel_t *model = ODEModel_createFromFile(model_file.get());
	if (!model) {
		cerr << "could not create ODE system from an SBML model: " << model_file.get() << endl;
		SolverError_dumpAndClearErrors();
		return false;
	}
	boost::scoped_ptr<Analysis> analysis(new Analysis(db_file, model));
	return analysis->Run();
}
