/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bcc.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_multi_pass.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "bc.pb.h"
#include "bc/pack.h"

using std::cerr;
using std::endl;
using std::make_pair;
using std::memcpy;

using namespace boost::spirit;

namespace flint {
namespace compiler {
namespace bcc {

struct UnaryCall {
	bc::Call1::Op op;
	int a1;
};

struct BinaryCall {
	bc::Call2::Op op;
	int a1;
	int a2;
};

struct UnaryGen {
	bc::Gen1::Type type;
	int a1;
};

struct BinaryGen {
	bc::Gen2::Type type;
	int a1;
	int a2;
};

enum {
	kOperationIsUnaryCall,
	kOperationIsBinaryCall,
	kOperationIsUnaryGen,
	kOperationIsBinaryGen
};

typedef boost::variant<UnaryCall, BinaryCall, UnaryGen, BinaryGen> Operation;

struct InstOp {
	int a;
	Operation operation;
};

struct InstBr {
	int a;
	int l;
};

struct InstLb {
	int a;
	std::string v;
	int d;
};

struct InstLd {
	int a;
	int i0;
	int i1;
	int d;
};

struct InstLoad {
	int a;
	std::string v;
};

struct InstLoadi {
	int a;
	double v;
};

struct InstStore {
	std::string v;
	int a;
};

struct Body {
	std::vector<bc::Code> code;
	std::vector<int> labels;
};

struct Block {
	int GetCodeSize() const {
		return static_cast<int>(code.size());
	}

	void Print(std::ostream *os) const {
		for (std::vector<bc::Code>::const_iterator it=code.begin();it!=code.end();++it) {
			PackToOstream(*it, os);
		}
	}

	std::vector<bc::Code> code;
	boost::uuids::uuid uuid;
	std::string name;
	int nod;
};

}
}
}

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::UnaryCall,
						  (bc::Call1::Op, op)
						  (int, a1))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::BinaryCall,
						  (bc::Call2::Op, op)
						  (int, a1)
						  (int, a2))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::UnaryGen,
						  (bc::Gen1::Type, type)
						  (int, a1))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::BinaryGen,
						  (bc::Gen2::Type, type)
						  (int, a1)
						  (int, a2))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstOp,
						  (int, a)
						  (flint::compiler::bcc::Operation, operation))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstBr,
						  (int, a)
						  (int, l))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstLb,
						  (int, a)
						  (std::string, v)
						  (int, d))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstLd,
						  (int, a)
						  (int, i0)
						  (int, i1)
						  (int, d))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstLoad,
						  (int, a)
						  (std::string, v))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstLoadi,
						  (int, a)
						  (double, v))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::InstStore,
						  (std::string, v)
						  (int, a))

BOOST_FUSION_ADAPT_STRUCT(flint::compiler::bcc::Body,
						  (std::vector<bc::Code>, code)
						  (std::vector<int>, labels))

namespace flint {
namespace compiler {
namespace bcc {
namespace {

typedef std::vector<Block> BlockVector;

template<typename TLexer>
struct Lexer : lex::lexer<TLexer> {

	Lexer() {
		this->self.add_pattern
			("DIGIT", "[0-9]")
			("SIGN", "[-+]")
			("EXPONENT", "[eE]{SIGN}?{DIGIT}+")
			("FLOAT", "{SIGN}?({DIGIT}*\".\"{DIGIT}+{EXPONENT}?|{DIGIT}+{EXPONENT})")
			;

		br_ = "br";
		jmp_ = "jmp";
		lb_ = "lb";
		ld_ = "ld";
		load_ = "load";
		loadi_ = "loadi";
		ret_ = "ret";
		store_ = "store";

		abs_ = "abs";
		arccos_ = "arccos";
		arccosh_ = "arccosh";
		arccot_ = "arccot";
		arccoth_ = "arccoth";
		arccsc_ = "arccsc";
		arccsch_ = "arccsch";
		arcsec_ = "arcsec";
		arcsech_ = "arcsech";
		arcsin_ = "arcsin";
		arcsinh_ = "arcsinh";
		arctan_ = "arctan";
		arctanh_ = "arctanh";
		ceiling_ = "ceiling";
		cos_ = "cos";
		cosh_ = "cosh";
		cot_ = "cot";
		coth_ = "coth";
		csc_ = "csc";
		csch_ = "csch";
		divide_ = "divide";
		eq_ = "eq";
		exp_ = "exp";
		factorial_ = "factorial";
		floor_ = "floor";
		geq_ = "geq";
		gt_ = "gt";
		leq_ = "leq";
		ln_ = "ln";
		log_ = "log";
		log10_ = "log10";
		lt_ = "lt";
		max_ = "max";
		min_ = "min";
		minus_ = "minus";
		neq_ = "neq";
		plus_ = "plus";
		power_ = "power";
		rem_ = "rem";
		root_ = "root";
		sec_ = "sec";
		sech_ = "sech";
		sin_ = "sin";
		sinh_ = "sinh";
		tan_ = "tan";
		tanh_ = "tanh";
		times_ = "times";

		exponential_variate_ = "$exponential_variate";
		gamma_variate_ = "$gamma_variate";
		gauss_variate_ = "$gauss_variate";
		lognormal_variate_ = "$lognormal_variate";
		poisson_variate_ = "$poisson_variate";
		uniform_variate_ = "$uniform_variate";
		weibull_variate_ = "$weibull_variate";

		eulergamma_ = "eulergamma";
		exponentiale_ = "exponentiale";
		pi_ = "pi";

		true_ = "true";
		false_ = "false";

		real = "{FLOAT}";
		integer = "{SIGN}?{DIGIT}+";
		address = "\"$\"{DIGIT}+";
		label = "\"L\"{DIGIT}+";
		id = "[%@][a-zA-Z_][a-zA-Z_0-9:#]*";

		this->self = lex::token_def<>('\n') | '\r' | '(' | ')' | ' ' | ':' | '=';
		this->self += br_ | jmp_ | lb_ | ld_ | load_ | loadi_ | ret_ | store_;
		this->self += abs_ | arccos_ | arccosh_ | arccot_ | arccoth_;
		this->self += arccsc_ | arccsch_ | arcsec_ | arcsech_;
		this->self += arcsin_ | arcsinh_ | arctan_ | arctanh_;
		this->self += ceiling_ | cos_ | cosh_ | cot_ | coth_;
		this->self += csc_ | csch_ | divide_ | eq_ | exp_ | factorial_ | floor_;
		this->self += geq_ | gt_ | leq_ | ln_ | log_ | log10_;
		this->self += lt_ | max_ | min_ | minus_ | neq_ | plus_;
		this->self += power_ | rem_ | root_ | sec_ | sech_ | sin_ | sinh_;
		this->self += tan_ | tanh_ | times_;
		this->self += exponential_variate_ | gamma_variate_ | gauss_variate_;
		this->self += lognormal_variate_ | poisson_variate_ | uniform_variate_;
		this->self += weibull_variate_;
		this->self += eulergamma_ | exponentiale_ | pi_;
		this->self += true_ | false_;
		this->self += real | integer | address | label | id;
	}

	lex::token_def<> br_, jmp_, lb_, ld_, load_, loadi_, ret_, store_;
	lex::token_def<> abs_, arccos_, arccosh_, arccot_, arccoth_;
	lex::token_def<> arccsc_, arccsch_, arcsec_, arcsech_;
	lex::token_def<> arcsin_, arcsinh_, arctan_, arctanh_;
	lex::token_def<> ceiling_, cos_, cosh_, cot_, coth_;
	lex::token_def<> csc_, csch_, divide_, eq_, exp_, factorial_, floor_;
	lex::token_def<> geq_, gt_, leq_, ln_, log_, log10_;
	lex::token_def<> lt_, max_, min_, minus_, neq_, plus_;
	lex::token_def<> power_, rem_, root_, sec_, sech_, sin_, sinh_;
	lex::token_def<> tan_, tanh_, times_;
	lex::token_def<> exponential_variate_, gamma_variate_, gauss_variate_;
	lex::token_def<> lognormal_variate_, poisson_variate_, uniform_variate_;
	lex::token_def<> weibull_variate_;
	lex::token_def<> eulergamma_, exponentiale_, pi_;
	lex::token_def<> true_, false_;
	lex::token_def<std::string> id;
	lex::token_def<int> integer;
	lex::token_def<std::string> address, label;
	lex::token_def<double> real;
};

void ProcessBody(Block &block, Body &body)
{
	int l;
	for (std::vector<bc::Code>::iterator it=body.code.begin();it!=body.code.end();++it) {
		switch (it->type()) {
		case bc::Code::kBr:
			{
				bc::Br *br = it->mutable_br();
				l = br->l();
				br->clear_l();
				br->set_p(body.labels.at(l));
			}
			break;
		case bc::Code::kJmp:
			{
				bc::Jmp *jmp = it->mutable_jmp();
				l = jmp->l();
				jmp->clear_l();
				jmp->set_p(body.labels.at(l));
			}
			break;
		default:
			break;
		}
	}
	block.code.swap(body.code);
}

void ProcessLabel(Body &body, const std::string &label)
{
	int l = std::atoi(label.c_str()+1);
	assert(l >= 0);
	if (body.labels.size() <= static_cast<size_t>(l))
		body.labels.resize(l+1);
	body.labels.at(l) = static_cast<int>(body.code.size());
}

void ProcessInstOp(bc::Code &code, const InstOp &inst_op)
{
	switch (inst_op.operation.which()) {
	case kOperationIsUnaryCall:
		{
			const UnaryCall &unary_call(boost::get<UnaryCall>(inst_op.operation));
			code.set_type(bc::Code::kCall1);
			bc::Call1 *call1 = code.mutable_call1();
			call1->set_a(inst_op.a);
			call1->set_op(unary_call.op);
			call1->set_a1(unary_call.a1);
		}
		break;
	case kOperationIsBinaryCall:
		{
			const BinaryCall &binary_call(boost::get<BinaryCall>(inst_op.operation));
			code.set_type(bc::Code::kCall2);
			bc::Call2 *call2 = code.mutable_call2();
			call2->set_a(inst_op.a);
			call2->set_op(binary_call.op);
			call2->set_a1(binary_call.a1);
			call2->set_a2(binary_call.a2);
		}
		break;
	case kOperationIsUnaryGen:
		{
			const UnaryGen &unary_gen(boost::get<UnaryGen>(inst_op.operation));
			code.set_type(bc::Code::kGen1);
			bc::Gen1 *gen1 = code.mutable_gen1();
			gen1->set_a(inst_op.a);
			gen1->set_type(unary_gen.type);
			gen1->set_a1(unary_gen.a1);
		}
		break;
	case kOperationIsBinaryGen:
		{
			const BinaryGen &binary_gen(boost::get<BinaryGen>(inst_op.operation));
			code.set_type(bc::Code::kGen2);
			bc::Gen2 *gen2 = code.mutable_gen2();
			gen2->set_a(inst_op.a);
			gen2->set_type(binary_gen.type);
			gen2->set_a1(binary_gen.a1);
			gen2->set_a2(binary_gen.a2);
		}
		break;
	default:
		assert(false);
		break;
	}
}

void ProcessInstBr(bc::Code &code, const InstBr &inst_br)
{
	code.set_type(bc::Code::kBr);
	bc::Br *br = code.mutable_br();
	br->set_a(inst_br.a);
	br->set_l(inst_br.l);
}

void ProcessInstJmp(bc::Code &code, int l)
{
	code.set_type(bc::Code::kJmp);
	bc::Jmp *jmp = code.mutable_jmp();
	jmp->set_l(l);
}

void ProcessInstLb(bc::Code &code, const InstLb &inst_lb)
{
	code.set_type(bc::Code::kLb);
	bc::Lb *lb = code.mutable_lb();
	lb->set_a(inst_lb.a);
	lb->set_v(inst_lb.v);
	lb->set_d(inst_lb.d);
}

void ProcessInstLd(bc::Code &code, const InstLd &inst_ld)
{
	code.set_type(bc::Code::kLd);
	bc::Ld *ld = code.mutable_ld();
	ld->set_a(inst_ld.a);
	ld->set_i0(inst_ld.i0);
	ld->set_i1(inst_ld.i1);
	ld->set_d(inst_ld.d);
}

void ProcessInstLoad(bc::Code &code, const InstLoad &inst_load)
{
	code.set_type(bc::Code::kLoad);
	bc::Load *load = code.mutable_load();
	load->set_a(inst_load.a);
	load->set_v(inst_load.v); // TODO
}

void ProcessInstLoadi(bc::Code &code, const InstLoadi &inst_loadi)
{
	code.set_type(bc::Code::kLoadi);
	bc::Loadi *loadi = code.mutable_loadi();
	loadi->set_a(inst_loadi.a);
	loadi->set_v(inst_loadi.v);
}

void ProcessInstStore(bc::Code &code, const InstStore &inst_store)
{
	code.set_type(bc::Code::kStore);
	bc::Store *store = code.mutable_store();
	store->set_v(inst_store.v);
	store->set_a(inst_store.a);
}

void ProcessAddress(InstOp &inst_op, const std::string &s)
{
	int a = std::atoi(s.c_str()+1);
	inst_op.a = a;
}

void ProcessTail(int &x, const std::string &s)
{
	x = std::atoi(s.c_str()+1);
}

template<typename TIterator>
struct Grammar : qi::grammar<TIterator, Body()> {

	template<typename TTokenDef>
	Grammar(TTokenDef const &td)
	: Grammar::base_type(body)
	{
		using boost::phoenix::at_c;
		using boost::phoenix::push_back;
		using boost::phoenix::val;
		using boost::spirit::qi::eol;

		body = +( ' '
				  >> ( (' ' >> inst [push_back(at_c<0>(_val), _1)])
					   | (td.label [bind(&ProcessLabel, _val, _1)] >> ':')
					   )
				  >> eol );

		inst = inst_op [bind(&ProcessInstOp, _val, _1)]
			| inst_br [bind(&ProcessInstBr, _val, _1)]
			| inst_jmp [bind(&ProcessInstJmp, _val, _1)]
			| inst_lb [bind(&ProcessInstLb, _val, _1)]
			| inst_ld [bind(&ProcessInstLd, _val, _1)]
			| inst_load [bind(&ProcessInstLoad, _val, _1)]
			| inst_loadi [bind(&ProcessInstLoadi, _val, _1)]
			| td.ret_ [bind(&bc::Code::set_type, _val, val(bc::Code::kRet))]
			| inst_store [bind(&ProcessInstStore, _val, _1)]
			;

		inst_op = td.address [bind(&ProcessAddress, _val, _1)]
			>> ' ' >> '=' >> ' ' >> '(' >> operation [at_c<1>(_val) = _1] >> ')';

		inst_br = td.br_ >> address [at_c<0>(_val) = _1]
						 >> label [at_c<1>(_val) = _1];

		inst_jmp = td.jmp_ >> label [_val = _1];

		inst_lb = td.lb_ >> address [at_c<0>(_val) = _1]
						 >> ' ' >> td.id [at_c<1>(_val) = _1]
						 >> address [at_c<2>(_val) = _1];

		inst_ld = td.ld_ >> address [at_c<0>(_val) = _1]
						 >> ' ' >> td.integer [at_c<1>(_val) = _1]
						 >> ' ' >> td.integer [at_c<2>(_val) = _1]
						 >> address [at_c<3>(_val) = _1];

		inst_load = td.load_ >> address [at_c<0>(_val) = _1]
							 >> ' ' >> td.id [at_c<1>(_val) = _1];

		inst_loadi = td.loadi_ >> address [at_c<0>(_val) = _1]
							   >> imm [at_c<1>(_val) = _1];

		inst_store = td.store_ >> ' ' >> td.id [at_c<0>(_val) = _1]
							   >> address [at_c<1>(_val) = _1];

		operation %= binary_call | unary_call | binary_gen | unary_gen;

		unary_call %= call1_op >> address;

		binary_call %= call2_op >> address >> address;

		unary_gen %= gen1_type >> address;

		binary_gen %= gen2_type >> address >> address;

		call1_op = td.abs_ [_val = val(bc::Call1::kAbs)]
			| td.arccos_ [_val = val(bc::Call1::kArccos)]
			| td.arccosh_ [_val = val(bc::Call1::kArccosh)]
			| td.arccot_ [_val = val(bc::Call1::kArccot)]
			| td.arccoth_ [_val = val(bc::Call1::kArccoth)]
			| td.arccsc_ [_val = val(bc::Call1::kArccsc)]
			| td.arccsch_ [_val = val(bc::Call1::kArccsch)]
			| td.arcsec_ [_val = val(bc::Call1::kArcsec)]
			| td.arcsech_ [_val = val(bc::Call1::kArcsech)]
			| td.arcsin_ [_val = val(bc::Call1::kArcsin)]
			| td.arcsinh_ [_val = val(bc::Call1::kArcsinh)]
			| td.arctan_ [_val = val(bc::Call1::kArctan)]
			| td.arctanh_ [_val = val(bc::Call1::kArctanh)]
			| td.ceiling_ [_val = val(bc::Call1::kCeiling)]
			| td.cos_ [_val = val(bc::Call1::kCos)]
			| td.cosh_ [_val = val(bc::Call1::kCosh)]
			| td.cot_ [_val = val(bc::Call1::kCot)]
			| td.coth_ [_val = val(bc::Call1::kCoth)]
			| td.csc_ [_val = val(bc::Call1::kCsc)]
			| td.csch_ [_val = val(bc::Call1::kCsch)]
			| td.exp_ [_val = val(bc::Call1::kExp)]
			| td.floor_ [_val = val(bc::Call1::kFloor)]
			| td.factorial_ [_val = val(bc::Call1::kFactorial)]
			| td.ln_ [_val = val(bc::Call1::kLn)]
			| td.log10_ [_val = val(bc::Call1::kLog10)]
			| td.minus_ [_val = val(bc::Call1::kMinus1)]
			| td.root_ [_val = val(bc::Call1::kRoot1)]
			| td.sec_ [_val = val(bc::Call1::kSec)]
			| td.sech_ [_val = val(bc::Call1::kSech)]
			| td.sin_ [_val = val(bc::Call1::kSin)]
			| td.sinh_ [_val = val(bc::Call1::kSinh)]
			| td.tan_ [_val = val(bc::Call1::kTan)]
			| td.tanh_ [_val = val(bc::Call1::kTanh)]
			;

		call2_op = td.divide_ [_val = val(bc::Call2::kDivide)]
			| td.eq_ [_val = val(bc::Call2::kEq)]
			| td.geq_ [_val = val(bc::Call2::kGeq)]
			| td.gt_ [_val = val(bc::Call2::kGt)]
			| td.leq_ [_val = val(bc::Call2::kLeq)]
			| td.log_ [_val = val(bc::Call2::kLog)]
			| td.lt_ [_val = val(bc::Call2::kLt)]
			| td.max_ [_val = val(bc::Call2::kMax)]
			| td.min_ [_val = val(bc::Call2::kMin)]
			| td.minus_ [_val = val(bc::Call2::kMinus2)]
			| td.neq_ [_val = val(bc::Call2::kNeq)]
			| td.plus_ [_val = val(bc::Call2::kPlus)]
			| td.power_ [_val = val(bc::Call2::kPower)]
			| td.rem_ [_val = val(bc::Call2::kRemainder)]
			| td.root_ [_val = val(bc::Call2::kRoot2)]
			| td.times_ [_val = val(bc::Call2::kTimes)]
			;

		gen1_type = td.exponential_variate_ [_val = val(bc::Gen1::kExponentialVariate)]
			| td.poisson_variate_ [_val = val(bc::Gen1::kPoissonVariate)]
			;

		gen2_type =td.gamma_variate_ [_val = val(bc::Gen2::kGammaVariate)]
			| td.gauss_variate_ [_val = val(bc::Gen2::kGaussVariate)]
			| td.lognormal_variate_ [_val = val(bc::Gen2::kLognormalVariate)]
			| td.uniform_variate_ [_val = val(bc::Gen2::kUniformVariate)]
			| td.weibull_variate_ [_val = val(bc::Gen2::kWeibullVariate)]
			;

		address = ' ' >> td.address [bind(&ProcessTail, _val, _1)];

		label = ' ' >> td.label [bind(&ProcessTail, _val, _1)];

		imm = ' ' >> (td.eulergamma_ [_val = val(boost::math::constants::euler<double>())]
					  | td.exponentiale_ [_val = val(boost::math::constants::e<double>())]
					  | td.pi_ [_val = val(boost::math::constants::pi<double>())]
					  | td.true_ [_val = val(1)]
					  | td.false_ [_val = val(0)]
					  | td.real [_val = _1]
					  | td.integer [_val = _1]
					  );
	}

	qi::rule<TIterator, Body()> body;
	qi::rule<TIterator, bc::Code()> inst;
	qi::rule<TIterator, InstOp()> inst_op;
	qi::rule<TIterator, InstBr()> inst_br;
	qi::rule<TIterator, int()> inst_jmp;
	qi::rule<TIterator, InstLb()> inst_lb;
	qi::rule<TIterator, InstLd()> inst_ld;
	qi::rule<TIterator, InstLoad()> inst_load;
	qi::rule<TIterator, InstLoadi()> inst_loadi;
	qi::rule<TIterator, InstStore()> inst_store;
	qi::rule<TIterator, Operation()> operation;
	qi::rule<TIterator, UnaryCall()> unary_call;
	qi::rule<TIterator, BinaryCall()> binary_call;
	qi::rule<TIterator, UnaryGen()> unary_gen;
	qi::rule<TIterator, BinaryGen()> binary_gen;
	qi::rule<TIterator, bc::Call1::Op()> call1_op;
	qi::rule<TIterator, bc::Call2::Op()> call2_op;
	qi::rule<TIterator, bc::Gen1::Type()> gen1_type;
	qi::rule<TIterator, bc::Gen2::Type()> gen2_type;
	qi::rule<TIterator, int()> address, label;
	qi::rule<TIterator, double()> imm;
};

typedef std::vector<std::pair<boost::uuids::uuid, int> > NobVector;

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

	explicit Parser(BlockVector *bv)
		: tokens_()
		, grammar_(tokens_)
		, bv_(bv)
	{
	}

	int Parse(const boost::uuids::uuid &uuid, const char *name, int nod, const char *code) {
		base_iterator_type it = code;
		base_iterator_type eit = code + std::strlen(code);
		Body body;
		bool r = lex::tokenize_and_parse(it, eit, tokens_, grammar_, body);
		if (!r || it != eit) {
			cerr << "failed to parse: " << it << endl;
			return 1;
		}
		Block block;
		block.uuid = uuid;
		block.name = name;
		block.nod = nod;
		ProcessBody(block, body);
		bv_->push_back(block);
		return 0;
	}

private:
	RealLexer tokens_;
	RealGrammar grammar_;
	BlockVector *bv_;
};

int Process(void *data, int argc, char **argv, char **names)
{
	(void)names;
	Parser *parser = static_cast<Parser *>(data);
	assert(argc == 4);
	assert(argv[0]);
	boost::uuids::uuid uuid;
	memcpy(&uuid, argv[0], uuid.size());
	const char *name = argv[1];
	int nod = std::atoi(argv[2]);
	const char *code = argv[3];
	return parser->Parse(uuid, name, nod, code);
}

int SetNol(void *data, int argc, char **argv, char **names)
{
	(void)names;
	assert(argc == 1);
	bc::Header *header = (bc::Header *)data;
	int nol = std::atoi(argv[0]);
	header->set_nol(nol);
	return 0;
}

}

bool Bcc(sqlite3 *db, std::ostream *os)
{
	BlockVector bv;
	{
		Parser parser(&bv);
		char *em;
		int e;
		e = sqlite3_exec(db, "SELECT * FROM tacs", Process, &parser, &em);
		if (e != SQLITE_OK) {
			if (e != SQLITE_ABORT)
				cerr << "failed to enumerate tacs: " << e << ": " << em << endl;
			sqlite3_free(em);
			return false;
		}
	}

	std::unique_ptr<NobVector> nv(new NobVector);
	for (BlockVector::const_iterator it=bv.begin();it!=bv.end();++it) {
		if (nv->empty()) {
			nv->push_back(make_pair(it->uuid, 1));
		} else if (nv->back().first == it->uuid) {
			nv->back().second++;
		} else {
			nv->push_back(make_pair(it->uuid, 1));
		}
	}
	// write header
	std::unique_ptr<bc::Header> header(new bc::Header);
	{
		char *em;
		int e;
		e = sqlite3_exec(db, "SELECT * FROM nol", SetNol, header.get(), &em);
		if (e != SQLITE_OK) {
			if (e != SQLITE_ABORT)
				cerr << "failed to select nol: " << e << ": " << em << endl;
			sqlite3_free(em);
			return false;
		}
	}
	header->set_nos(static_cast<int>(nv->size()));
	if (!PackToOstream(*header, os)) {
		return false;
	}
	// write section headers
	std::unique_ptr<bc::SectionHeader> sh(new bc::SectionHeader);
	std::unique_ptr<char[]> bu(new char[boost::uuids::uuid::static_size()]);
	for (NobVector::const_iterator nit=nv->begin();nit!=nv->end();++nit) {
		const boost::uuids::uuid &uuid(nit->first);
		std::copy(uuid.begin(), uuid.end(), bu.get());
		sh->set_id(bu.get(), uuid.size());
		sh->set_nob(nit->second);
		if (!PackToOstream(*sh, os)) {
			return false;
		}
	}
	// write block headers
	std::unique_ptr<bc::BlockHeader> bh(new bc::BlockHeader);
	for (BlockVector::const_iterator it=bv.begin();it!=bv.end();++it) {
		bh->set_name(it->name);
		bh->set_nod(it->nod);
		bh->set_noc(it->GetCodeSize());
		if (!PackToOstream(*bh, os)) {
			return false;
		}
	}
	// write body
	for (BlockVector::const_iterator it=bv.begin();it!=bv.end();++it) {
		it->Print(os);
	}

	return true;
}

}
}
}
