/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "phml.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <libxml/xmlreader.h>

#include "branch.hh"
#include "db.hh"
#include "db/driver.hh"
#include "db/helper.h"
#include "db/query.h"
#include "flint/utf8string.h"
#include "modelpath.h"
#include "phml/definition_dumper.h"
#include "phml/delay-arg-validator.hh"
#include "phml/graph-iv-rewriter.h"
#include "phml/graph-math-rewriter.h"
#include "phml/graph_reader.h"
#include "phml/transition-form.h"
#include "reach.hh"
#include "reduction.hh"
#include "span.hh"
#include "sprinkle.hh"
#include "sqlite3.h"
#include "ts.hh"
#include "utf8path.h"

using std::atoi;
using std::cerr;
using std::endl;
using std::memcpy;
using std::printf;
using std::set;
using std::string;

namespace flint {
namespace phml {

namespace {

class NumericalConfiguration {
public:
	NumericalConfiguration(const NumericalConfiguration &) = delete;
	NumericalConfiguration &operator=(const NumericalConfiguration &) = delete;

	NumericalConfiguration()
		: rg_name_(NULL),
		  rg_seed_(NULL),
		  integration_(NULL),
		  sts_unit_id_(),
		  sts_value_(NULL)
	{}

	~NumericalConfiguration() {
		if (rg_name_) xmlFree(rg_name_);
		if (rg_seed_) xmlFree(rg_seed_);
		if (integration_) xmlFree(integration_);
		if (sts_value_) xmlFree(sts_value_);
	}

	const xmlChar *rg_name() const {return rg_name_;}
	void set_rg_name(xmlChar *rg_name) {rg_name_ = rg_name;}
	const xmlChar *rg_seed() const {return rg_seed_;}
	void set_rg_seed(xmlChar *rg_seed) {rg_seed_ = rg_seed;}
	const xmlChar *integration() const {return integration_;}
	void set_integration(xmlChar *integration) {integration_ = integration;}
	int sts_unit_id() const {return sts_unit_id_;}
	void set_sts_unit_id(int sts_unit_id) {sts_unit_id_ = sts_unit_id;}
	const xmlChar *sts_value() const {return sts_value_;}
	void set_sts_value(xmlChar *sts_value) {sts_value_ = sts_value;}

private:
	xmlChar *rg_name_;
	xmlChar *rg_seed_;
	xmlChar *integration_;
	int sts_unit_id_;
	xmlChar *sts_value_;
};

class TimeDiscretization {
public:
	TimeDiscretization(const TimeDiscretization &) = delete;
	TimeDiscretization &operator=(const TimeDiscretization &) = delete;

	TimeDiscretization()
		: unit_id_(),
		  step_(NULL),
		  module_id_(NULL)
	{
	}

	~TimeDiscretization() {
		if (step_) xmlFree(step_);
		if (module_id_) xmlFree(module_id_);
	}

	int unit_id() const {return unit_id_;}
	void set_unit_id(int unit_id) {unit_id_ = unit_id;}
	const xmlChar *step() const {return step_;}
	void set_step(xmlChar *step) {step_ = step;}

	const xmlChar *module_id() const {return module_id_;}
	void set_module_id(xmlChar *module_id) {module_id_ = module_id;}

private:
	int unit_id_;
	xmlChar *step_;
	xmlChar *module_id_;
};

class Unit {
public:
	Unit(const Unit &) = delete;
	Unit &operator=(const Unit &) = delete;

	explicit Unit(int unit_id)
	: unit_id_(unit_id),
	  name_(NULL),
	  rowid_()
	{}

	~Unit() {
		if (name_) xmlFree(name_);
	}

	int unit_id() const {return unit_id_;}
	const xmlChar *name() const {return name_;}
	void set_name(xmlChar *name) {name_ = name;}
	sqlite3_int64 rowid() const {return rowid_;}
	void set_rowid(sqlite3_int64 rowid) {rowid_ = rowid;}

private:
	int unit_id_;
	xmlChar *name_;
	sqlite3_int64 rowid_;
};

class Element {
public:
	Element(const Element &) = delete;
	Element &operator=(const Element &) = delete;

	Element()
	: unit_id_(),
	  exponent_(),
	  factor_(),
	  multiplier_(),
	  offset_()
	{}

	int unit_id() const {return unit_id_;}
	void set_unit_id(int unit_id) {unit_id_ = unit_id;}
	double exponent() const {return exponent_;}
	void set_exponent(double exponent) {exponent_ = exponent;}
	int factor() const {return factor_;}
	void set_factor(int factor) {factor_ = factor;}
	double multiplier() const {return multiplier_;}
	void set_multiplier(double multiplier) {multiplier_ = multiplier;}
	double offset() const {return offset_;}
	void set_offset(double offset) {offset_ = offset;}

private:
	int unit_id_;
	double exponent_;
	int factor_;
	double multiplier_;
	double offset_;
};

class Module {
public:
	Module(const Module &) = delete;
	Module &operator=(const Module &) = delete;

	Module()
	: module_id_(NULL),
	  type_(NULL),
	  name_(NULL),
	  capsulated_by_(NULL),
	  template_state_(NULL),
	  rowid_()
	{}

	~Module() {
		if (module_id_) xmlFree(module_id_);
		if (type_) xmlFree(type_);
		if (name_) xmlFree(name_);
		if (capsulated_by_) xmlFree(capsulated_by_);
		if (template_state_) xmlFree(template_state_);
	}

	const xmlChar *module_id() const {return module_id_;}
	void set_module_id(xmlChar *module_id) {module_id_ = module_id;}
	const xmlChar *type() const {return type_;}
	void set_type(xmlChar *type) {type_ = type;}
	const xmlChar *name() const {return name_;}
	void set_name(xmlChar *name) {name_ = name;}
	const xmlChar *capsulated_by() const {return capsulated_by_;}
	void set_capsulated_by(xmlChar *capsulated_by) {capsulated_by_ = capsulated_by;}
	const xmlChar *template_state() const {return template_state_;}
	void set_template_state(xmlChar *template_state) {template_state_ = template_state;}

	sqlite3_int64 rowid() const {return rowid_;}
	void set_rowid(sqlite3_int64 rowid) {rowid_ = rowid;}

	boost::uuids::uuid GetUuid() const {
		boost::uuids::string_generator gen;
		return gen((const char *)module_id_);
	}

	boost::uuids::uuid GetCapsulatedBy() const {
		assert(capsulated_by_);
		boost::uuids::string_generator gen;
		return gen((const char *)capsulated_by_);
	}

	bool IsValid() const {
		return module_id_ && name_;
	}

	bool IsSaved() const {
		return rowid_ != 0;
	}

private:
	xmlChar *module_id_;
	xmlChar *type_;
	xmlChar *name_;
	xmlChar *capsulated_by_;
	xmlChar *template_state_;
	sqlite3_int64 rowid_;
};

class Bridge {
public:
	Bridge(const Bridge &) = delete;
	Bridge &operator=(const Bridge &) = delete;

	Bridge() : sub_type_(NULL), direction_(NULL), connector_(NULL) {}

	~Bridge() {
		if (sub_type_) xmlFree(sub_type_);
		if (direction_) xmlFree(direction_);
		if (connector_) xmlFree(connector_);
	}

	const xmlChar *sub_type() const {return sub_type_;}
	void set_sub_type(xmlChar *sub_type) {sub_type_ = sub_type;}
	const xmlChar *direction() const {return direction_;}
	void set_direction(xmlChar *direction) {direction_ = direction;}
	const xmlChar *connector() const {return connector_;}
	void set_connector(xmlChar *connector) {connector_ = connector;}

private:
	xmlChar *sub_type_;
	xmlChar *direction_;
	xmlChar *connector_;
};

class Port {
public:
	Port(const Port &) = delete;
	Port &operator=(const Port &) = delete;

	Port() : port_id_(NULL), direction_(NULL), ref_pq_id_(NULL), multiple_(NULL) {}

	~Port() {
		if (port_id_) xmlFree(port_id_);
		if (direction_) xmlFree(direction_);
		if (ref_pq_id_) xmlFree(ref_pq_id_);
		if (multiple_) xmlFree(multiple_);
	}

	const xmlChar *port_id() const {return port_id_;}
	void set_port_id(xmlChar *port_id) {port_id_ = port_id;}
	const xmlChar *direction() const {return direction_;}
	void set_direction(xmlChar *direction) {direction_ = direction;}
	const xmlChar *ref_pq_id() const {return ref_pq_id_;}
	void set_ref_pq_id(xmlChar *ref_pq_id) {ref_pq_id_ = ref_pq_id;}
	const xmlChar *multiple() const {return multiple_;}
	void set_multiple(xmlChar *multiple) {multiple_ = multiple;}

private:
	xmlChar *port_id_;
	xmlChar *direction_;
	xmlChar *ref_pq_id_;
	xmlChar *multiple_;
};

class Edge {
public:
	enum Type {
		kFunctional,
		kForwarding,
	};

	Edge(const Edge &) = delete;
	Edge &operator=(const Edge &) = delete;

	explicit Edge(Type type)
	: type_(type ),
	  tail_module_id_(),
	  tail_port_id_(),
	  head_module_id_(),
	  head_port_id_()
	{}

	~Edge() {
		if (tail_module_id_) xmlFree(tail_module_id_);
		if (tail_port_id_) xmlFree(tail_port_id_);
		if (head_module_id_) xmlFree(head_module_id_);
		if (head_port_id_) xmlFree(head_port_id_);
	}

	const xmlChar *tail_module_id() const {return tail_module_id_;}
	void set_tail_module_id(xmlChar *tail_module_id) {tail_module_id_ = tail_module_id;}
	const xmlChar *tail_port_id() const {return tail_port_id_;}
	void set_tail_port_id(xmlChar *tail_port_id) {tail_port_id_ = tail_port_id;}
	const xmlChar *head_module_id() const {return head_module_id_;}
	void set_head_module_id(xmlChar *head_module_id) {head_module_id_ = head_module_id;}
	const xmlChar *head_port_id() const {return head_port_id_;}
	void set_head_port_id(xmlChar *head_port_id) {head_port_id_ = head_port_id;}

	boost::uuids::uuid GetUuidOfTailModuleId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)tail_module_id_);
	}

	boost::uuids::uuid GetUuidOfHeadModuleId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)head_module_id_);
	}

private:
	Type type_;
	xmlChar *tail_module_id_;
	xmlChar *tail_port_id_;
	xmlChar *head_module_id_;
	xmlChar *head_port_id_;
};

class Import {
public:
	enum Format {
		kUnspecifiedFormat,
		kSbml
	};

	Import(const Import &) = delete;
	Import &operator=(const Import &) = delete;

	Import() : type_(NULL), format_(kUnspecifiedFormat), iref_(NULL), xref_(NULL), zref_(NULL) {}
	~Import() {
		if (type_) xmlFree(type_);
		if (iref_) xmlFree(iref_);
		if (xref_) xmlFree(xref_);
		if (zref_) xmlFree(zref_);
	}

	const xmlChar *type() const {return type_;}
	void set_type(xmlChar *type) {type_ = type;}
	Format format() const {return format_;}
	void set_format(Format format) {format_ = format;}
	const xmlChar *iref() const {return iref_;}
	void set_iref(xmlChar * iref) {iref_ = iref;}
	const xmlChar *xref() const {return xref_;}
	void set_xref(xmlChar * xref) {xref_ = xref;}
	const xmlChar *zref() const {return zref_;}
	void set_zref(xmlChar * zref) {zref_ = zref;}

private:
	xmlChar *type_;
	Format format_;
	xmlChar *iref_;
	xmlChar *xref_;
	xmlChar *zref_;
};

class Instance {
public:
	Instance(const Instance &) = delete;
	Instance &operator=(const Instance &) = delete;

	Instance()
		: module_id_(NULL),
		  template_id_(NULL),
		  label_(NULL),
		  rowid_()
	{}

	~Instance() {
		if (module_id_) xmlFree(module_id_);
		if (template_id_) xmlFree(template_id_);
		if (label_) xmlFree(label_);
	}

	const xmlChar *module_id() const {return module_id_;}
	const xmlChar *template_id() const {return template_id_;}
	const xmlChar *label() const {return label_;}
	void set_module_id(xmlChar *module_id) {module_id_ = module_id;}
	void set_template_id(xmlChar *template_id) {template_id_ = template_id;}
	void set_label(xmlChar *label) {label_ = label;}

	sqlite3_int64 rowid() const {return rowid_;}
	void set_rowid(sqlite3_int64 rowid) {rowid_ = rowid;}

	boost::uuids::uuid GetUuidOfModuleId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)module_id_);
	}

	boost::uuids::uuid GetUuidOfTemplateId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)template_id_);
	}

	bool IsSaved() const {
		return rowid_ != 0;
	}

private:
	xmlChar *module_id_;
	xmlChar *template_id_;
	xmlChar *label_;
	sqlite3_int64 rowid_;
};

class PQ {
public:
	enum Type {
		kUnknown,
		kState,
		kVariableParameter,
		kStaticParameter,
		kTimeseries
	};

	PQ(const PQ &) = delete;
	PQ &operator=(const PQ &) = delete;

	PQ()
		: type_(kUnknown),
		  pq_id_(),
		  unit_id_(NULL),
		  name_(NULL),
		  col_(0),
		  row_(0),
		  max_delay_(NULL),
		  rowid_()
	{}

	~PQ() {
		if (unit_id_) xmlFree(unit_id_);
		if (name_) xmlFree(name_);
		if (max_delay_) xmlFree(max_delay_);
	}

	Type type() const {return type_;}
	void set_type(Type type) {type_ = type;}
	int pq_id() const {return pq_id_;}
	void set_pq_id(int pq_id) {pq_id_ = pq_id;}
	const xmlChar *unit_id() const {return unit_id_;}
	void set_unit_id(xmlChar *unit_id) {unit_id_ = unit_id;}
	const xmlChar *name() const {return name_;}
	void set_name(xmlChar *name) {name_ = name;}
	int col() const {return col_;}
	void set_col(int col) {col_ = col;}
	int row() const {return row_;}
	void set_row(int row) {row_ = row;}
	const xmlChar *max_delay() const {return max_delay_;}
	void set_max_delay(xmlChar *max_delay) {max_delay_ = max_delay;}
	sqlite3_int64 rowid() const {return rowid_;}
	void set_rowid(sqlite3_int64 rowid) {rowid_ = rowid;}

	bool IsValid() const {
		return type_ != kUnknown && pq_id_ > 0;
	}

	const char *GetType() const {
		switch (type_) {
		case kState:             return "x";
		case kVariableParameter: return "v";
		case kStaticParameter:   return "s";
		case kTimeseries:        return "t";
		default:
			assert(false);
			return NULL;
		}
	}

	const char *GetTypeName() const {
		switch (type_) {
		case kState:             return "state";
		case kVariableParameter: return "variable-parameter";
		case kStaticParameter:   return "static-parameter";
		case kTimeseries:        return "timeseries";
		default:
			assert(false);
			return NULL;
		}
	}

	bool IsSaved() const {
		return rowid_ != 0;
	}

private:
	Type type_;
	int pq_id_;
	xmlChar *unit_id_;
	xmlChar *name_;
	int col_;
	int row_;
	xmlChar *max_delay_;
	sqlite3_int64 rowid_;
};

class InitialValue {
public:
	static const char *kName;

	InitialValue(const InitialValue &) = delete;
	InitialValue &operator=(const InitialValue &) = delete;

	InitialValue() {}

	void OpenDefinition(int /*level*/) const {}

	void CloseDefinition(int /*level*/) const {}

	std::ostream *GetOutputStream() {
		return &stream_;
	}

	void Print(const char *s) {
		stream_ << s;
	}

	string GetString() const {
		return stream_.str();
	}

	bool IsProper() const {
		return !stream_.str().empty();
	}

private:
	std::ostringstream stream_;
};

const char *InitialValue::kName = "initial-value";

class Implementation {
public:
	static const char *kName;

	Implementation(const Implementation &) = delete;
	Implementation &operator=(const Implementation &) = delete;

	Implementation() {}

	void OpenDefinition(int /*level*/) const {}

	void CloseDefinition(int /*level*/) const {}

	std::ostream *GetOutputStream() {
		return &stream_;
	}

	void Print(const char *s) {
		stream_ << s;
	}

	string GetString() const {
		return stream_.str();
	}

	bool IsProper() const {
		return !stream_.str().empty();
	}

private:
	std::ostringstream stream_;
};

const char *Implementation::kName = "implementation";

class Definition {
public:
	Definition(const Definition &) = delete;
	Definition &operator=(const Definition &) = delete;

	Definition()
		: type_(NULL),
		  sub_type_(NULL),
		  format_(NULL)
	{}

	~Definition() {
		if (type_) xmlFree(type_);
		if (sub_type_) xmlFree(sub_type_);
		if (format_) xmlFree(format_);
	}

	const xmlChar *type() const {return type_;}
	const xmlChar *sub_type() const {return sub_type_;}
	const xmlChar *format() const {return format_;}
	void set_type(xmlChar *type) {type_ = type;}
	void set_sub_type(xmlChar *sub_type) {sub_type_ = sub_type;}
	void set_format(xmlChar *format) {format_ = format;}

private:
	xmlChar *type_;
	xmlChar *sub_type_;
	xmlChar *format_;
};

class Reference {
public:
	Reference(const Reference &) = delete;
	Reference &operator=(const Reference &) = delete;

	Reference()
		: port_id_(),
		  timeseries_id_(),
		  element_id_(NULL)
	{}

	~Reference() {
		if (element_id_) xmlFree(element_id_);
	}

	int port_id() const {return port_id_;}
	int timeseries_id() const {return timeseries_id_;}
	const xmlChar *element_id() const {return element_id_;}

	void set_port_id(int port_id) {port_id_ = port_id;}
	void set_timeseries_id(int timeseries_id) {timeseries_id_ = timeseries_id;}
	void set_element_id(xmlChar *element_id) {element_id_ = element_id;}

private:
	int port_id_;
	int timeseries_id_;
	xmlChar *element_id_;
};

class ExtraImplementation {
public:
	ExtraImplementation(const ExtraImplementation &) = delete;
	ExtraImplementation &operator=(const ExtraImplementation &) = delete;

	static const char *kName;

	ExtraImplementation()
		: type_(nullptr)
		, order_(nullptr)
	{
	}

	~ExtraImplementation() {
		if (type_) xmlFree(type_);
		if (order_) xmlFree(order_);
	}

	const xmlChar *type() const {return type_;}
	void set_type(xmlChar *type) {type_ = type;}
	const xmlChar *order() const {return order_;}
	void set_order(xmlChar *order) {order_ = order;}
	const Definition *definition() const {return definition_.get();}
	void set_definition(Definition *definition) {definition_.reset(definition);}

	void OpenDefinition(int /*level*/) const {}

	void CloseDefinition(int /*level*/) const {}

	std::ostream *GetOutputStream() {
		return &stream_;
	}

	void Print(const char *s) {
		stream_ << s;
	}

	string GetString() const {
		return stream_.str();
	}

	bool IsProper() const {
		return !stream_.str().empty();
	}

private:
	xmlChar *type_;
	xmlChar *order_;
	std::unique_ptr<Definition> definition_;
	std::ostringstream stream_;
};

const char *ExtraImplementation::kName = "extra-implementation";

class Template {
public:
	Template(const Template &) = delete;
	Template &operator=(const Template &) = delete;

	Template() : template_id_(NULL), ref_module_id_(NULL) {}

	~Template() {
		if (template_id_) xmlFree(template_id_);
		if (ref_module_id_) xmlFree(ref_module_id_);
	}

	const xmlChar *template_id() const {return template_id_;}
	void set_template_id(xmlChar *template_id) {template_id_ = template_id;}
	const xmlChar *ref_module_id() const {return ref_module_id_;}
	void set_ref_module_id(xmlChar *ref_module_id) {ref_module_id_ = ref_module_id;}

	boost::uuids::uuid GetUuidOfTemplateId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)template_id_);
	}

	boost::uuids::uuid GetUuidOfRefModuleId() const {
		boost::uuids::string_generator gen;
		return gen((const char *)ref_module_id_);
	}

private:
	xmlChar *template_id_;
	xmlChar *ref_module_id_;
};

class TargetModule {
public:
	TargetModule(const TargetModule &) = delete;
	TargetModule &operator=(const TargetModule &) = delete;

	explicit TargetModule(xmlChar *module_id)
	: module_id_(module_id),
	  rowid_()
	{}

	~TargetModule() {
		if (module_id_) xmlFree(module_id_);
	}

	const xmlChar *module_id() const {return module_id_;}

	sqlite3_int64 rowid() const {return rowid_;}
	void set_rowid(sqlite3_int64 rowid) {rowid_ = rowid;}

	boost::uuids::uuid GetUuid() const {
		boost::uuids::string_generator gen;
		return gen((const char *)module_id_);
	}

	bool IsSaved() const {
		return rowid_ != 0;
	}

private:
	xmlChar *module_id_;
	sqlite3_int64 rowid_;
};

class TargetPq {
public:
	static const char *kName;

	TargetPq(const TargetPq &) = delete;
	TargetPq &operator=(const TargetPq &) = delete;

	explicit TargetPq(int pq_id)
	: pq_id_(pq_id),
	  stream_()
	{}

	int pq_id() const {return pq_id_;}

	void OpenDefinition(int /*level*/) const {}

	void CloseDefinition(int /*level*/) const {}

	std::ostream *GetOutputStream() {
		return &stream_;
	}

	void Print(const char *s) {
		stream_ << s;
	}

	string GetString() const {
		return stream_.str();
	}

	bool IsProper() const {
		return !stream_.str().empty();
	}

private:
	int pq_id_;
	std::ostringstream stream_;
};

const char *TargetPq::kName = "target-physical-quantity";

class Timeseries {
public:
	Timeseries(const Timeseries &) = delete;
	Timeseries &operator=(const Timeseries &) = delete;

	Timeseries()
		: timeseries_id_(NULL),
		  format_(NULL),
		  iref_(NULL),
		  zref_(NULL)
	{}

	~Timeseries() {
		if (timeseries_id_) xmlFree(timeseries_id_);
		if (format_) xmlFree(format_);
		if (iref_) xmlFree(iref_);
		if (zref_) xmlFree(zref_);
	}

	const xmlChar *timeseries_id() const {return timeseries_id_;}
	void set_timeseries_id(xmlChar *timeseries_id) {timeseries_id_ = timeseries_id;}
	const xmlChar *format() const {return format_;}
	void set_format(xmlChar *format) {format_ = format;}
	const xmlChar *iref() const {return iref_;}
	void set_iref(xmlChar *iref) {iref_ = iref;}
	const xmlChar *zref() const {return zref_;}
	void set_zref(xmlChar *zref) {zref_ = zref;}

private:
	xmlChar *timeseries_id_;
	xmlChar *format_;
	xmlChar *iref_;
	xmlChar *zref_;
};

template<typename TElement>
bool GetAbsolutePathFromReference(const TElement *element,
								  const boost::filesystem::path &given_path,
								  const boost::filesystem::path &model_path,
								  boost::filesystem::path *output_path)
{
	if (element->iref()) {
		boost::filesystem::path iref_path = GetPathFromUtf8((const char *)element->iref());
		if (iref_path.is_absolute()) {
			*output_path = iref_path;
			return true;
		}
		boost::filesystem::path gpp(given_path.parent_path());
		*output_path = boost::filesystem::absolute(iref_path, gpp);
		return true;
	}
	if (element->zref()) {
		boost::filesystem::path zref_path = GetPathFromUtf8((const char *)element->zref());
		if (zref_path.is_absolute()) {
			cerr << "found an absolute zref: "
				 << element->zref()
				 << endl;
			return false;
		}
		boost::filesystem::path mpp(model_path.parent_path());
		*output_path = boost::filesystem::absolute(zref_path, mpp);
		return true;
	}
	return false;
}

class DatabaseDriver {
public:
	DatabaseDriver(const DatabaseDriver &) = delete;
	DatabaseDriver &operator=(const DatabaseDriver &) = delete;

	explicit DatabaseDriver(sqlite3 *db)
		: db_(db)
	{
		int e;

		// prepare statements
		e = sqlite3_prepare_v2(db, "INSERT INTO ncs VALUES (?, ?, ?, ?, ?)",
							   -1, &nc_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO tds VALUES (?, ?, ?)",
							   -1, &td_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO units VALUES (?, ?)",
							   -1, &unit_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO elements VALUES (?, ?, ?, ?, ?, ?)",
							   -1, &element_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO modules VALUES (?, ?, ?, ?, ?)",
							   -1, &module_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO pqs (module_rowid, type, pq_id) VALUES (?, ?, ?)",
							   -1, &pq_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO ivs VALUES (?, ?)",
							   -1, &iv_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO impls VALUES (?, ?)",
							   -1, &impl_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO nodes VALUES (?, ?, ?)",
							   -1, &node_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO arcs VALUES (?, ?, ?, ?, ?)",
							   -1, &arc_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO refports VALUES (?, ?, ?)",
							   -1, &refport_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO refts VALUES (?, ?, ?)",
							   -1, &refts_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO extras VALUES (?, ?, ?)",
							   -1, &extra_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO edges VALUES (?, ?, ?, ?)",
							   -1, &edge_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO bridges VALUES (?, ?, ?, ?)",
							   -1, &bridge_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO imports VALUES (?, ?, ?)",
							   -1, &import_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO instances VALUES (?, ?, ?)",
							   -1, &instance_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO tms VALUES (?, ?)",
							   -1, &tm_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO tpqs VALUES (?, ?, ?)",
							   -1, &tpq_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO templates VALUES (?, ?)",
							   -1, &template_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO ports VALUES (?, ?, ?, ?, ?)",
							   -1, &port_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "INSERT INTO timeseries VALUES (?, ?, ?, ?)",
							   -1, &timeseries_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db, "UPDATE pqs SET unit_id = ?, name = ?, ncols = ?, nrows = ?, max_delay = ? WHERE rowid = ?",
							   -1, &update_stmt_[kPq], NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}
	}

	~DatabaseDriver() {
		sqlite3_finalize(nc_stmt_);
		sqlite3_finalize(td_stmt_);
		sqlite3_finalize(unit_stmt_);
		sqlite3_finalize(element_stmt_);
		sqlite3_finalize(module_stmt_);
		sqlite3_finalize(pq_stmt_);
		sqlite3_finalize(iv_stmt_);
		sqlite3_finalize(impl_stmt_);
		sqlite3_finalize(node_stmt_);
		sqlite3_finalize(arc_stmt_);
		sqlite3_finalize(refport_stmt_);
		sqlite3_finalize(refts_stmt_);
		sqlite3_finalize(extra_stmt_);
		sqlite3_finalize(edge_stmt_);
		sqlite3_finalize(bridge_stmt_);
		sqlite3_finalize(import_stmt_);
		sqlite3_finalize(instance_stmt_);
		sqlite3_finalize(tm_stmt_);
		sqlite3_finalize(tpq_stmt_);
		sqlite3_finalize(template_stmt_);
		sqlite3_finalize(port_stmt_);
		sqlite3_finalize(timeseries_stmt_);
		sqlite3_finalize(update_stmt_[kPq]);
	}

	bool SaveNumericalConfiguration(const NumericalConfiguration *nc) {
		int e;
		e = sqlite3_bind_text(nc_stmt_, 1, (const char *)nc->rg_name(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind rg_name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(nc_stmt_, 2, (const char *)nc->rg_seed(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind rg_seed: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(nc_stmt_, 3, (const char *)nc->integration(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind integration: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(nc_stmt_, 4, nc->sts_unit_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind sts_unit_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(nc_stmt_, 5, (const char *)nc->sts_value(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind sts_value: " << e << endl;
			return false;
		}
		e = sqlite3_step(nc_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(nc_stmt_);
		return true;
	}

	bool SaveTimeDiscretization(const TimeDiscretization *td, const Module *module = NULL) {
		int e;
		e = sqlite3_bind_int(td_stmt_, 1, td->unit_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind unit_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(td_stmt_, 2, (const char *)td->step(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind step: " << e << endl;
			return false;
		}
		boost::uuids::uuid u;
		if (module) {
			u = module->GetUuid();
			e = sqlite3_bind_blob(td_stmt_, 3, &u, u.size(), SQLITE_STATIC);
		} else {
			e = sqlite3_bind_null(td_stmt_, 3);
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_id: " << e << endl;
			return false;
		}
		e = sqlite3_step(td_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(td_stmt_);
		return true;
	}

	bool SaveUnit(Unit *unit) {
		int e;
		e = sqlite3_bind_int(unit_stmt_, 1, unit->unit_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind unit_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(unit_stmt_, 2, (const char *)unit->name(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_step(unit_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(unit_stmt_);

		sqlite3_int64 rowid = sqlite3_last_insert_rowid(db_);
		unit->set_rowid(rowid);

		return true;
	}

	bool SaveElement(const Unit *unit, const Element *element) {
		enum {
			kUnitRowid = 1,
			kUnitId,
			kExponent,
			kFactor,
			kMultiplier,
			kOffset
		};

		int e;
		e = sqlite3_bind_int64(element_stmt_, kUnitRowid, unit->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind unit_rowid: " << e << endl;
			return false;
		}

		e = sqlite3_bind_int(element_stmt_, kUnitId, element->unit_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind unit_id: " << e << endl;
			return false;
		}

		if (element->exponent() == 0) {
			e = sqlite3_bind_null(element_stmt_, kExponent);
		} else {
			e = sqlite3_bind_double(element_stmt_, kExponent, element->exponent());
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind exponent: " << e << endl;
			return false;
		}

		if (element->factor() == 0) {
			e = sqlite3_bind_null(element_stmt_, kFactor);
		} else {
			e = sqlite3_bind_int(element_stmt_, kFactor, element->factor());
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind factor: " << e << endl;
			return false;
		}

		if (element->multiplier() == 0) {
			e = sqlite3_bind_null(element_stmt_, kMultiplier);
		} else {
			e = sqlite3_bind_double(element_stmt_, kMultiplier, element->multiplier());
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind multiplier: " << e << endl;
			return false;
		}

		if (element->offset() == 0) {
			e = sqlite3_bind_null(element_stmt_, kOffset);
		} else {
			e = sqlite3_bind_double(element_stmt_, kOffset, element->offset());
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind offset: " << e << endl;
			return false;
		}

		e = sqlite3_step(element_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(element_stmt_);
		return true;
	}

	bool SaveModule(Module *module) {
		if (!module->IsValid()) {
			cerr << "module is invalid" << endl;
			return false;
		}

		int e;
		boost::uuids::uuid u = module->GetUuid();
		e = sqlite3_bind_blob(module_stmt_, 1, &u, u.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(module_stmt_, 2, (const char *)module->type(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind type: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(module_stmt_, 3, (const char *)module->name(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		boost::uuids::uuid cu;
		if (module->capsulated_by()) {
			cu = module->GetCapsulatedBy();
			e = sqlite3_bind_blob(module_stmt_, 4, &cu, cu.size(), SQLITE_STATIC);
		} else {
			e = sqlite3_bind_null(module_stmt_, 4);
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind capsulated_by: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(module_stmt_, 5, (const char *)module->template_state(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind template_state: " << e << endl;
			return false;
		}
		e = sqlite3_step(module_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(module_stmt_);

		sqlite3_int64 rowid = sqlite3_last_insert_rowid(db_);
		module->set_rowid(rowid);

		return true;
	}

	bool SavePq(const Module *module, PQ *pq) {
		if (!module->IsSaved()) {
			cerr << "module is not saved yet: "
				 << module->module_id()
				 << endl;
			return false;
		}
		if (!pq->IsValid()) {
			cerr << "pq is invalid" << endl;
			return false;
		}

		int e;
		e = sqlite3_bind_int64(pq_stmt_, 1, module->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(pq_stmt_, 2, (const char *)pq->GetType(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind type: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(pq_stmt_, 3, pq->pq_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_id: " << e << endl;
			return false;
		}
		e = sqlite3_step(pq_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(pq_stmt_);

		sqlite3_int64 rowid = sqlite3_last_insert_rowid(db_);
		pq->set_rowid(rowid);

		return true;
	}

	bool UpdatePq(const PQ *pq) {
		assert(pq->IsSaved());
		sqlite3_stmt *stmt = update_stmt_[kPq];
		int e;
		e = sqlite3_bind_text(stmt, 1, (const char *)pq->unit_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind unit_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt, 2, (const char *)pq->name(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(stmt, 3, pq->col());
		if (e != SQLITE_OK) {
			cerr << "failed to bind ncols: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(stmt, 4, pq->row());
		if (e != SQLITE_OK) {
			cerr << "failed to bind nrows: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(stmt, 5, (const char *)pq->max_delay(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind max_delay: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int64(stmt, 6, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind rowid: " << e << endl;
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

	bool SaveInitialValue(const PQ *pq, const InitialValue *iv) {
		int e;
		e = sqlite3_bind_int64(iv_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		string math = iv->GetString();
		e = sqlite3_bind_text(iv_stmt_, 2, math.c_str(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(iv_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(iv_stmt_);
		return true;
	}

	bool SaveImplementation(const PQ *pq, const Implementation *impl) {
		int e;
		e = sqlite3_bind_int64(impl_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		string math = impl->GetString();
		e = sqlite3_bind_text(impl_stmt_, 2, math.c_str(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(impl_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(impl_stmt_);
		return true;
	}

	template<typename TNode>
	bool SaveNode(const PQ *pq, const TNode *node) {
		int e;
		e = sqlite3_bind_int64(node_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(node_stmt_, 2, node->node_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind node_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(node_stmt_, 3, (const char *)node->name(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind name: " << e << endl;
			return false;
		}
		e = sqlite3_step(node_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(node_stmt_);
		return true;
	}

	template<typename TArc>
	bool SaveArc(const PQ *pq, const TArc *arc) {
		int e;
		e = sqlite3_bind_int64(arc_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(arc_stmt_, 2, arc->tail_node_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind tail_node_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(arc_stmt_, 3, arc->head_node_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind head_node_id: " << e << endl;
			return false;
		}
		assert(arc->type() != TArc::kUnspecified);
		e = sqlite3_bind_text(arc_stmt_, 4, (arc->type() == TArc::kCondition) ? "condition" : "probability", -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind type: " << e << endl;
			return false;
		}
		string math = arc->GetMath();
		e = sqlite3_bind_text(arc_stmt_, 5, math.c_str(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(arc_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(arc_stmt_);
		return true;
	}

	bool SaveReference(const PQ *pq, const Reference *reference, const ExtraImplementation *extra) {
		int e;
		if (reference->port_id() > 0) { // reference to port
			e = sqlite3_bind_int64(refport_stmt_, 1, pq->rowid());
			if (e != SQLITE_OK) {
				cerr << "failed to bind pq_rowid: " << e << endl;
				return false;
			}
			e = sqlite3_bind_int(refport_stmt_, 2, reference->port_id());
			if (e != SQLITE_OK) {
				cerr << "failed to bind port_id: " << e << endl;
				return false;
			}
			if ( extra &&
				 extra->definition() &&
				 xmlStrEqual(extra->definition()->type(), BAD_CAST "reduction") ) {
				Reduction reduction;
				if (!ConvertStringToReduction((const char *)extra->definition()->sub_type(),
											  &reduction))
					return false;
				e = sqlite3_bind_int(refport_stmt_, 3, static_cast<int>(reduction));
			} else {
				// assume "sum" by default, for backward compatibility
				e = sqlite3_bind_int(refport_stmt_, 3, static_cast<int>(Reduction::kSum));
			}
			e = sqlite3_step(refport_stmt_);
			if (e != SQLITE_DONE) {
				cerr << "failed to step statement: " << e << endl;
				return false;
			}
			sqlite3_reset(refport_stmt_);
		} else { // reference to timeseries
			e = sqlite3_bind_int64(refts_stmt_, 1, pq->rowid());
			if (e != SQLITE_OK) {
				cerr << "failed to bind pq_rowid: " << e << endl;
				return false;
			}
			e = sqlite3_bind_int(refts_stmt_, 2, reference->timeseries_id());
			if (e != SQLITE_OK) {
				cerr << "failed to bind timeseries_id: " << e << endl;
				return false;
			}
			e = sqlite3_bind_text(refts_stmt_, 3, (const char *)reference->element_id(), -1, SQLITE_STATIC);
			if (e != SQLITE_OK) {
				cerr << "failed to bind element_id: " << e << endl;
				return false;
			}
			e = sqlite3_step(refts_stmt_);
			if (e != SQLITE_DONE) {
				cerr << "failed to step statement: " << e << endl;
				return false;
			}
			sqlite3_reset(refts_stmt_);
		}
		return true;
	}

	bool SaveExtraImplementation(const PQ *pq, const ExtraImplementation *extra) {
		int e;
		e = sqlite3_bind_int64(extra_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(extra_stmt_, 2, (const char *)extra->order(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind order_type: " << e << endl;
			return false;
		}
		string math = extra->GetString();
		e = sqlite3_bind_text(extra_stmt_, 3, math.c_str(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(extra_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(extra_stmt_);
		return true;
	}

	bool SaveEdge(const Edge *edge) {
		int e;
		boost::uuids::uuid tu = edge->GetUuidOfTailModuleId();
		boost::uuids::uuid hu = edge->GetUuidOfHeadModuleId();
		e = sqlite3_bind_blob(edge_stmt_, 1, &tu, tu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind tail_module_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(edge_stmt_, 2, (const char *)edge->tail_port_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind tail_port_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_blob(edge_stmt_, 3, &hu, hu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind head_module_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(edge_stmt_, 4, (const char *)edge->head_port_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind head_port_id: " << e << endl;
			return false;
		}
		e = sqlite3_step(edge_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(edge_stmt_);
		return true;
	}

	bool SaveBridge(const PQ *pq, const Bridge *bridge) {
		int e;
		e = sqlite3_bind_int64(bridge_stmt_, 1, pq->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(bridge_stmt_, 2, (const char *)bridge->direction(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind direction: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(bridge_stmt_, 3, (const char *)bridge->sub_type(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind sub_type: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(bridge_stmt_, 4, (const char *)bridge->connector(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind connector: " << e << endl;
			return false;
		}
		e = sqlite3_step(bridge_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(bridge_stmt_);
		return true;
	}

	bool SaveImport(const Module *module, const Import *import,
					const boost::filesystem::path &given_path,
					const boost::filesystem::path &model_path) {
		if (!module->IsSaved()) {
			cerr << "module is not saved yet: "
				 << module->module_id()
				 << endl;
			return false;
		}

		int e;
		e = sqlite3_bind_int64(import_stmt_, 1, module->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(import_stmt_, 2, (const char *)import->type(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind type: " << e << endl;
			return false;
		}

		std::unique_ptr<char[]> utf8;
		if (xmlStrEqual(import->type(), BAD_CAST "external")) {
			boost::filesystem::path path;
			if (!GetAbsolutePathFromReference(import, given_path, model_path, &path)) {
				cerr << "external <import> without reference" << endl;
				return false;
			}
			utf8.reset(GetUtf8FromPath(path));
			e = sqlite3_bind_text(import_stmt_, 3, utf8.get(), -1, SQLITE_STATIC);
		} else {
			e = sqlite3_bind_null(import_stmt_, 3);
		}
		if (e != SQLITE_OK) {
			cerr << "failed to bind ref: " << e << endl;
			return false;
		}

		e = sqlite3_step(import_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(import_stmt_);
		return true;
	}

	bool SaveInstance(Instance *instance) {
		int e;
		boost::uuids::uuid mu = instance->GetUuidOfModuleId();
		boost::uuids::uuid tu = instance->GetUuidOfTemplateId();
		e = sqlite3_bind_blob(instance_stmt_, 1, &mu, mu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_blob(instance_stmt_, 2, &tu, tu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind template_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(instance_stmt_, 3, (const char *)instance->label(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind label: " << e << endl;
			return false;
		}
		e = sqlite3_step(instance_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(instance_stmt_);

		sqlite3_int64 rowid = sqlite3_last_insert_rowid(db_);
		instance->set_rowid(rowid);

		return true;
	}

	bool SaveTargetModule(const Instance *instance, TargetModule *tm) {
		if (!instance->IsSaved()) {
			cerr << "instance is not saved yet: "
				 << instance->module_id()
				 << endl;
			return false;
		}

		int e;
		e = sqlite3_bind_int64(tm_stmt_, 1, instance->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind instance_rowid: " << e << endl;
			return false;
		}
		boost::uuids::uuid u = tm->GetUuid();
		e = sqlite3_bind_blob(tm_stmt_, 2, &u, u.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_id: " << e << endl;
			return false;
		}
		e = sqlite3_step(tm_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(tm_stmt_);

		sqlite3_int64 rowid = sqlite3_last_insert_rowid(db_);
		tm->set_rowid(rowid);

		return true;
	}

	bool SaveTargetPq(const TargetModule *tm, const TargetPq *tpq) {
		int e;
		e = sqlite3_bind_int64(tpq_stmt_, 1, tm->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind tm_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int(tpq_stmt_, 2, tpq->pq_id());
		if (e != SQLITE_OK) {
			cerr << "failed to bind pq_id: " << e << endl;
			return false;
		}
		string math = tpq->GetString();
		e = sqlite3_bind_text(tpq_stmt_, 3, (const char *)math.c_str(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind math: " << e << endl;
			return false;
		}
		e = sqlite3_step(tpq_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(tpq_stmt_);
		return true;
	}

	bool SaveTemplate(const Template *t) {
		int e;
		boost::uuids::uuid tu = t->GetUuidOfTemplateId();
		boost::uuids::uuid rmu = t->GetUuidOfRefModuleId();
		e = sqlite3_bind_blob(template_stmt_, 1, &tu, tu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind template_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_blob(template_stmt_, 2, &rmu, rmu.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind ref_module_id: " << e << endl;
			return false;
		}
		e = sqlite3_step(template_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(template_stmt_);
		return true;
	}

	bool SavePort(const Module *module, const Port *port) {
		if (!module->IsSaved()) {
			cerr << "module is not saved yet: "
				 << module->module_id()
				 << endl;
			return false;
		}

		int e;
		e = sqlite3_bind_int64(port_stmt_, 1, module->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(port_stmt_, 2, (const char *)port->port_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind port_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(port_stmt_, 3, (const char *)port->direction(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind direction: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(port_stmt_, 4, (const char *)port->ref_pq_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind ref_pq_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(port_stmt_, 5, (const char *)port->multiple(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind multiple: " << e << endl;
			return false;
		}
		e = sqlite3_step(port_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(port_stmt_);
		return true;
	}

	bool SaveTimeseries(const Module *module, const Timeseries *ts,
						const boost::filesystem::path &given_path,
						const boost::filesystem::path &model_path) {
		if (!module->IsSaved()) {
			cerr << "module is not saved yet: "
				 << module->module_id()
				 << endl;
			return false;
		}

		int e;
		e = sqlite3_bind_int64(timeseries_stmt_, 1, module->rowid());
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_rowid: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(timeseries_stmt_, 2, (const char *)ts->timeseries_id(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind timeseries_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_text(timeseries_stmt_, 3, (const char *)ts->format(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind format: " << e << endl;
			return false;
		}

		boost::filesystem::path path;
		if (!GetAbsolutePathFromReference(ts, given_path, model_path, &path)) {
			cerr << "timeseries without reference" << endl;
			return false;
		}
		std::unique_ptr<char[]> utf8(GetUtf8FromPath(path));
		e = sqlite3_bind_text(timeseries_stmt_, 4, utf8.get(), -1, SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind ref: " << e << endl;
			return false;
		}
		e = sqlite3_step(timeseries_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(timeseries_stmt_);
		return true;
	}

private:
	enum UpdateStatementIndex {
		kPq,
		kNumOfUpdateStatements
	};

	sqlite3 *db_;
	sqlite3_stmt *nc_stmt_;
	sqlite3_stmt *td_stmt_;
	sqlite3_stmt *unit_stmt_;
	sqlite3_stmt *element_stmt_;
	sqlite3_stmt *module_stmt_;
	sqlite3_stmt *pq_stmt_;
	sqlite3_stmt *iv_stmt_;
	sqlite3_stmt *impl_stmt_;
	sqlite3_stmt *node_stmt_;
	sqlite3_stmt *arc_stmt_;
	sqlite3_stmt *refport_stmt_;
	sqlite3_stmt *refts_stmt_;
	sqlite3_stmt *extra_stmt_;
	sqlite3_stmt *edge_stmt_;
	sqlite3_stmt *bridge_stmt_;
	sqlite3_stmt *import_stmt_;
	sqlite3_stmt *instance_stmt_;
	sqlite3_stmt *tm_stmt_;
	sqlite3_stmt *tpq_stmt_;
	sqlite3_stmt *template_stmt_;
	sqlite3_stmt *port_stmt_;
	sqlite3_stmt *timeseries_stmt_;
	sqlite3_stmt *update_stmt_[kNumOfUpdateStatements];
};

class CapsulatedByValidator {
public:
	explicit CapsulatedByValidator(sqlite3 *db)
		: query_stmt_(NULL)
	{
		int e = sqlite3_prepare_v2(db, kQuery, -1, &query_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << kQuery << ": " << e << endl;
			exit(EXIT_FAILURE);
		}
	}

	~CapsulatedByValidator() {
		sqlite3_finalize(query_stmt_);
	}

	bool Validate() {
		bool r = true;
		int e;
		for (e = sqlite3_step(query_stmt_); e == SQLITE_ROW; e = sqlite3_step(query_stmt_)) {
			r = false;
			const void *module_id = sqlite3_column_blob(query_stmt_, 0);
			const void *capsulated_by = sqlite3_column_blob(query_stmt_, 1);
			assert(module_id);
			assert(capsulated_by);
			boost::uuids::uuid mu, cu;
			memcpy(&mu, module_id, mu.size());
			memcpy(&cu, capsulated_by, cu.size());
			cerr << "module of module-id " << mu
				 << " is capsulated by unknown capsule module: " << cu
				 << endl;
		}
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << kQuery << ": " << e << endl;
			return false;
		}
		sqlite3_reset(query_stmt_);
		return r;
	}

private:
	static const char kQuery[];

	sqlite3_stmt *query_stmt_;
};

const char CapsulatedByValidator::kQuery[] = "SELECT m0.module_id, m0.capsulated_by FROM modules AS m0 WHERE"
											 " m0.capsulated_by IS NOT NULL AND"
											 " NOT EXISTS (SELECT * FROM modules AS m1 WHERE m1.module_id = m0.capsulated_by)";

class TreeWriter {
public:
	TreeWriter(const TreeWriter &) = delete;
	TreeWriter &operator=(const TreeWriter &) = delete;

	explicit TreeWriter(sqlite3 *db)
		: db_(db),
		  query_stmt_(NULL),
		  tree_stmt_(NULL),
		  roots_(),
		  children_()
	{
		int e;

		e = sqlite3_prepare_v2(db_, "SELECT module_id, capsulated_by FROM modules WHERE type = 'capsule' OR type = 'functional-unit'",
							   -1, &query_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}

		e = sqlite3_prepare_v2(db_, "INSERT INTO trees VALUES (?, ?)",
							   -1, &tree_stmt_, NULL);
		if (e != SQLITE_OK) {
			cerr << "failed to prepare statement: " << e << endl;
			exit(EXIT_FAILURE);
		}
	}

	~TreeWriter() {
		sqlite3_finalize(query_stmt_);
		sqlite3_finalize(tree_stmt_);
	}

	bool Write() {
		int e;
		for (e = sqlite3_step(query_stmt_); e == SQLITE_ROW; e = sqlite3_step(query_stmt_)) {
			const void *module_id = sqlite3_column_blob(query_stmt_, 0);
			assert(module_id);
			assert(sqlite3_column_bytes(query_stmt_, 0) == boost::uuids::uuid::static_size());
			const void *capsulated_by = sqlite3_column_blob(query_stmt_, 1);
			boost::uuids::uuid child;
			memcpy(&child, module_id, child.size());
			if (capsulated_by) {
				assert(sqlite3_column_bytes(query_stmt_, 1) == boost::uuids::uuid::static_size());
				boost::uuids::uuid parent;
				memcpy(&parent, capsulated_by, parent.size());
				children_.insert(std::make_pair(parent, child));
			} else {
				roots_.push_back(child);
			}
		}
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(query_stmt_);

		for (Vector::const_iterator it=roots_.begin();it!=roots_.end();++it) {
			if (!SaveDescendants(*it, 0)) return false;
		}
		return true;
	}

private:
	typedef std::multimap<boost::uuids::uuid, boost::uuids::uuid> Multimap;
	typedef std::vector<boost::uuids::uuid> Vector;

	bool SaveDescendants(const boost::uuids::uuid &parent, int level) {
		std::pair<Multimap::const_iterator, Multimap::const_iterator> p;
		p = children_.equal_range(parent);
		for (Multimap::const_iterator it=p.first;it!=p.second;++it) {
			if (!SaveDescendants(it->second, level + 1)) return false;
		}
		return Save(parent, level);
	}

	bool Save(const boost::uuids::uuid &module_id, int level) {
		int e = sqlite3_bind_blob(tree_stmt_, 1, &module_id, module_id.size(), SQLITE_STATIC);
		if (e != SQLITE_OK) {
			cerr << "failed to bind module_id: " << e << endl;
			return false;
		}
		e = sqlite3_bind_int64(tree_stmt_, 2, level);
		if (e != SQLITE_OK) {
			cerr << "failed to bind level: " << e << endl;
			return false;
		}
		e = sqlite3_step(tree_stmt_);
		if (e != SQLITE_DONE) {
			cerr << "failed to step statement: " << e << endl;
			return false;
		}
		sqlite3_reset(tree_stmt_);
		return true;
	}

	sqlite3 *db_;
	sqlite3_stmt *query_stmt_;
	sqlite3_stmt *tree_stmt_;
	Vector roots_;
	Multimap children_;
};

class Reader {
public:
	Reader(const Reader &) = delete;
	Reader &operator=(const Reader &) = delete;

	Reader(const boost::filesystem::path &given_path,
		   const boost::filesystem::path &model_path,
		   xmlTextReaderPtr &text_reader,
		   sqlite3 *db)
		: given_path_(given_path),
		  model_path_(model_path),
		  text_reader_(text_reader),
		  dd_(new DatabaseDriver(db)),
		  module_(),
		  pq_(),
		  iv_(),
		  impl_(),
		  ref_(),
		  extra_(),
		  edge_(),
		  bridge_(),
		  instance_(),
		  iv_dumper_(),
		  impl_dumper_(),
		  extra_dumper_()
	{
	}

	~Reader() {
		xmlFreeTextReader(text_reader_);
		xmlCleanupParser();
	}

	int Read() {
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "header")) {
					i = ReadHeader();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "unit-set")) {
					i = ReadUnitSet();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "module-set")) {
					i = ReadModuleSet();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "template-set")) {
					i = ReadTemplateSet();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "instance-set")) {
					i = ReadInstanceSet();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "edge-set")) {
					i = ReadEdgeSet();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "edge-set")) {
					return 1;
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

private:
	void ReportHint() {
		if (module_ && module_->module_id() && module_->name()) {
			cerr << " at";
			if (pq_ && pq_->pq_id() > 0 && pq_->name()) {
				cerr << " physical-quantity \"" << pq_->name()
					 << "\" (" << pq_->pq_id() << ") of";
			}
			cerr << " module \"" << module_->name()
				 << "\" (" << module_->module_id()
				 << ')' << endl;
		}
	}

	int ReadHeader() {
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "numerical-configuration")) {
					i = ReadNumericalConfiguration();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "header")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadNumericalConfiguration() {
		std::unique_ptr<NumericalConfiguration> nc(new NumericalConfiguration);
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "time-discretization")) {
					i = ReadTimeDiscretization();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "algorithm")) {
					i = ReadAlgorithm(nc.get());
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "simulation-time-span")) {
					i = ReadSimulationTimeSpan(nc.get());
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <numerical-configuration>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "numerical-configuration")) {
					if (!dd_->SaveNumericalConfiguration(nc.get())) return -2;
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadTimeDiscretization() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no children, so we can ignore this <time-discretization>
			return xmlTextReaderRead(text_reader_);
		}
		std::unique_ptr<TimeDiscretization> td(new TimeDiscretization);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "unit-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				int unit_id = atoi((const char *)value);
				if (unit_id > 0) {
					td->set_unit_id(unit_id);
				} else {
					// generously ignore invalid unit-id
					cerr << "invalid unit-id of <time-discretization>: "
						 << unit_id << endl;
				}
			}
		}
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "step")) {
					i = ReadStep(td.get());
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "time-discretization")) {
					if (td->unit_id() && td->step()) {
						if (!dd_->SaveTimeDiscretization(td.get(), module_.get())) return -2;
					}
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadStep(TimeDiscretization *td) {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		if (!s) {
			// generously ignore empty step
			cerr << "missing body of <step>" << endl;
			return xmlTextReaderNext(text_reader_);
		}
		xmlChar *step;
		if (!Trim(s, &step)) {
			xmlFree(s);
			return -2;
		}
		int len = xmlStrlen(step);
		if (len == 0) {
			// generously ignore empty step
			cerr << "empty body of <step>" << endl;
			xmlFree(s);
			return xmlTextReaderNext(text_reader_);
		}

		// validate step
		if (ContainNonGraphic(step)) {
			cerr << "<step> contains invalid character: \"" << step << "\"" << endl;
			xmlFree(s);
			return -2;
		}

		td->set_step(xmlStrdup(step));
		xmlFree(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadAlgorithm(NumericalConfiguration *nc) {
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "random-generator")) {
					i = ReadRandomGenerator(nc);
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "integration")) {
					i = ReadIntegration(nc);
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <algorithm>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "algorithm")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadRandomGenerator(NumericalConfiguration *nc) {
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				// ignored
			} else if (xmlStrEqual(local_name, BAD_CAST "name")) {
				nc->set_rg_name(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "seed")) {
				nc->set_rg_seed(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <random-generator>: " << local_name << endl;
				return -2;
			}
		}
		if (!nc->rg_name()) {
			cerr << "missing name of <random-generator>" << endl;
			return -2;
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadIntegration(NumericalConfiguration *nc) {
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "name")) {
				nc->set_integration(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <integration>: " << local_name << endl;
				return -2;
			}
		}
		if (!nc->integration()) {
			cerr << "missing name of <integration>" << endl;
			return -2;
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadSimulationTimeSpan(NumericalConfiguration *nc) {
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "unit-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				int unit_id = atoi((const char *)value);
				if (unit_id <= 0) {
					cerr << "invalid unit-id of <simulation-time-span>: " << value << endl;
					return -2;
				}
				nc->set_sts_unit_id(unit_id);
			}
		}
		if (!nc->sts_unit_id()) {
			cerr << "missing unit-id of <simulation-time-span>" << endl;
			return -2;
		}

		i = xmlTextReaderRead(text_reader_);
		if (i <= 0) return i;
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		// validate value
		int len = xmlStrlen(s);
		for (int i=0;i<len;i++) {
			if (!isprint(s[i])) {
				cerr << "<simulation-time-span> contains invalid character: \"" << s << "\"" << endl;
				xmlFree(s);
				return -2;
			}
		}

		nc->set_sts_value(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadUnitSet() {
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "unit")) {
					i = ReadUnit();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <unit-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "unit-set")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadUnit() {
		xmlChar *value;
		int i = ReadAttributeValue(BAD_CAST "unit-id", &value);
		if (i <= 0) {
			cerr << "missing unit-id of <unit>" << endl;
			return -2;
		}
		int unit_id = atoi((const char *)value);
		xmlFree(value);
		if (unit_id < 0) {
			cerr << "invalid unit-id " << unit_id << endl;
			return -2;
		}
		std::unique_ptr<Unit> unit(new Unit(unit_id));
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "name")) {
					i = ReadUnitName(unit.get());
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "element")) {
					i = ReadElement(unit.get());
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <unit>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "unit")) {
					if (!unit->name()) {
						cerr << "missing <name> of <unit>" << endl;
						return -2;
					}
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadUnitName(Unit *unit) {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		// validate name
		int len = xmlStrlen(s);
		for (int i=0;i<len;i++) {
			if (!isprint(s[i])) {
				cerr << "unit name contains invalid character: \"" << s << "\"" << endl;
				xmlFree(s);
				return -2;
			}
		}

		unit->set_name(s);
		if (!dd_->SaveUnit(unit)) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadElement(const Unit *unit) {
		if (!unit->rowid()) {
			cerr << "<element> comes before <name> of <unit>" << endl;
			return -2;
		}

		std::unique_ptr<Element> element(new Element);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			const xmlChar *value = xmlTextReaderConstValue(text_reader_);
			if (!value) continue;

			if (xmlStrEqual(local_name, BAD_CAST "unit-id")) {
				int unit_id = atoi((char *)value);
				if (unit_id < 0) {
					cerr << "invalid unit-id of <element>" << endl;
					return -2;
				}
				if (unit_id == unit->unit_id()) {
					cerr << "unit with unit-id " << unit_id << " is ill-defined by <element> with its own unit-id" << endl;
					return -2;
				}
				element->set_unit_id(unit_id);
			} else if (xmlStrEqual(local_name, BAD_CAST "exponent")) {
				element->set_exponent(strtod((char *)value, NULL));
			} else if (xmlStrEqual(local_name, BAD_CAST "prefix")) {
				if (xmlStrEqual(value, BAD_CAST "yotta")) {
					element->set_factor(24);
				} else if (xmlStrEqual(value, BAD_CAST "zetta")) {
					element->set_factor(21);
				} else if (xmlStrEqual(value, BAD_CAST "exa")) {
					element->set_factor(18);
				} else if (xmlStrEqual(value, BAD_CAST "peta")) {
					element->set_factor(15);
				} else if (xmlStrEqual(value, BAD_CAST "tera")) {
					element->set_factor(12);
				} else if (xmlStrEqual(value, BAD_CAST "giga")) {
					element->set_factor(9);
				} else if (xmlStrEqual(value, BAD_CAST "mega")) {
					element->set_factor(6);
				} else if (xmlStrEqual(value, BAD_CAST "kilo")) {
					element->set_factor(3);
				} else if (xmlStrEqual(value, BAD_CAST "hecto")) {
					element->set_factor(2);
				} else if (xmlStrEqual(value, BAD_CAST "deca")) {
					element->set_factor(1);
				} else if (xmlStrEqual(value, BAD_CAST "deci")) {
					element->set_factor(-1);
				} else if (xmlStrEqual(value, BAD_CAST "centi")) {
					element->set_factor(-2);
				} else if (xmlStrEqual(value, BAD_CAST "milli")) {
					element->set_factor(-3);
				} else if (xmlStrEqual(value, BAD_CAST "micro")) {
					element->set_factor(-6);
				} else if (xmlStrEqual(value, BAD_CAST "nano")) {
					element->set_factor(-9);
				} else if (xmlStrEqual(value, BAD_CAST "pico")) {
					element->set_factor(-12);
				} else if (xmlStrEqual(value, BAD_CAST "femto")) {
					element->set_factor(-15);
				} else if (xmlStrEqual(value, BAD_CAST "atto")) {
					element->set_factor(-18);
				} else if (xmlStrEqual(value, BAD_CAST "zepto")) {
					element->set_factor(-21);
				} else if (xmlStrEqual(value, BAD_CAST "yocto")) {
					element->set_factor(-24);
				} else {
					cerr << "unknown prefix of <element>: " << value << endl;
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "multiplier")) {
				element->set_multiplier(strtod((char *)value, NULL));
			} else if (xmlStrEqual(local_name, BAD_CAST "offset")) {
				element->set_offset(strtod((char *)value, NULL));
			} else {
				cerr << "unknown attribute of <element>: " << local_name << endl;
				return -2;
			}
		}

		if (!element->unit_id()) {
			cerr << "missing unit-id of <element>" << endl;
			return -2;
		}
		if (!dd_->SaveElement(unit, element.get())) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadModuleSet() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no modules, so we have done
			return xmlTextReaderRead(text_reader_);
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "module")) {
					i = ReadModule();
					if (i <= 0) {
						ReportHint();
						return i;
					}
					continue;
				} else {
					cerr << "unknown child of <module-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "module-set")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadModule() {
		module_.reset(new Module);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "module-id")) {
				module_->set_module_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "type")) {
				module_->set_type(xmlTextReaderValue(text_reader_));
			}
		}
		if (!module_->module_id()) {
			cerr << "missing module-id of <module>" << endl;
			return -2;
		}
		if (!module_->type()) {
			cerr << "missing type of <module>: " << module_->module_id() << endl;
			return -2;
		}
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "property")) {
					i = ReadProperty();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "port")) {
					i = ReadPort();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "physical-quantity")) {
					i = ReadPQ();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "import")) {
					i = ReadImport();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "timeseries")) {
					i = ReadTimeseries();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "module")) {
					module_.reset();
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadProperty() {
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "name")) {
					i = ReadModuleName();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "capsulation")) {
					i = ReadCapsulation();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "template")) {
					i = ReadModuleTemplate();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "time-discretization")) {
					i = ReadTimeDiscretization();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "property")) {
					if (!dd_->SaveModule(module_.get())) return -2;
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadModuleName() {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		// validate name
		int len = xmlStrlen(s);
		for (int i=0;i<len;i++) {
			switch (s[i]) {
			case '\n':
			case '\r':
				cerr << "name contains invalid character: \"" << s << "\"" << endl;
				xmlFree(s);
				return -2;
			default:
				break;
			}
		}

		module_->set_name(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadCapsulation() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no children, so we have done
			return xmlTextReaderRead(text_reader_);
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "capsulated-by")) {
					i = ReadCapsulatedBy();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "capsulation")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadCapsulatedBy() {
		xmlChar *module_id;
		int i = ReadAttributeValue(BAD_CAST "module-id", &module_id);
		if (i <= 0) {
			cerr << "missing module-id of <capsulated-by>" << endl;
			return -2;
		}
		module_->set_capsulated_by(module_id);
		return xmlTextReaderRead(text_reader_);
	}

	int ReadModuleTemplate() {
		xmlChar *state;
		int i = ReadAttributeValue(BAD_CAST "state", &state);
		if (i <= 0) {
			cerr << "missing state of <template>" << endl;
			return -2;
		}
		module_->set_template_state(state);
		return xmlTextReaderRead(text_reader_);
	}

	int ReadPort() {
		std::unique_ptr<Port> port(new Port);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "port-id")) {
				port->set_port_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "direction")) {
				xmlChar *direction = xmlTextReaderValue(text_reader_);
				if (xmlStrEqual(direction, BAD_CAST "in")) {
					port->set_direction(direction);
				} else if (xmlStrEqual(direction, BAD_CAST "out")) {
					port->set_direction(direction);
				} else {
					cerr << "unknown direction of <port>: " << direction << endl;
					xmlFree(direction);
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "ref-physical-quantity-id")) {
				port->set_ref_pq_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "multiple")) {
				port->set_multiple(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <port>: " << local_name << endl;
				return -2;
			}
		}
		if (!port->port_id()) {
			cerr << "missing port-id of <port>" << endl;
			return -2;
		}
		if (!dd_->SavePort(module_.get(), port.get())) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadPQ() {
		pq_.reset(new PQ);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "physical-quantity-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				int pq_id = atoi((const char *)value);
				if (pq_id <= 0) {
					cerr << "invalid physical-quantity-id of <physical-quantity>: " << value << endl;
					return -2;
				}
				pq_->set_pq_id(pq_id);
			} else if (xmlStrEqual(local_name, BAD_CAST "type")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "state")) {
					pq_->set_type(PQ::kState);
				} else if (xmlStrEqual(value, BAD_CAST "variable-parameter")) {
					pq_->set_type(PQ::kVariableParameter);
				} else if (xmlStrEqual(value, BAD_CAST "static-parameter")) {
					pq_->set_type(PQ::kStaticParameter);
				} else if (xmlStrEqual(value, BAD_CAST "func-expression")) {
					// skip this type of PQ
					return i;
				} else if (xmlStrEqual(value, BAD_CAST "nominal")) {
					// skip this type of PQ
					return i;
				} else if (xmlStrEqual(value, BAD_CAST "morphology")) {
					// skip this type of PQ
					return i;
				} else if (xmlStrEqual(value, BAD_CAST "timeseries")) {
					pq_->set_type(PQ::kTimeseries);
				} else {
					cerr << "unknown type fo <physical-quantity>: " << value << endl;
					return -2;
				}
			}
		}
		if (pq_->pq_id() == 0) {
			cerr << "missing physical-quantity-id of <physical-quantity>" << endl;
			return -2;
		}
		if (pq_->type() == PQ::kUnknown) {
			cerr << "missing type of <physical-quantity>: " << pq_->pq_id() << endl;
			return -2;
		}
		if (!dd_->SavePq(module_.get(), pq_.get())) return -2;
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "name")) {
					i = ReadPqName();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "dimension")) {
					i = ReadDimension();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "max-delay")) {
					i = ReadMaxDelay();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "value-type-set")) {
					i = ReadValueTypeSet();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "initial-value")) {
					i = ReadInitialValue();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "implementation")) {
					i = ReadImplementation();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "physical-quantity")) {
					if (!pq_->name()) {
						cerr << "missing <name> of <physical-quantity>: " << pq_->pq_id() << endl;
						return -2;
					}
					if (pq_->col() == 0 && pq_->row() == 0) {
						cerr << "missing <dimension> of <physical-quantity>" << endl;
						return -2;
					}
					if (!dd_->UpdatePq(pq_.get())) return -2;
					if (iv_dumper_) {
						if (pq_->type() != PQ::kState) {
							cerr << "unexpected <initial-value> for <physical-quantity> of type "
								 << pq_->GetTypeName()
								 << endl;
							return -2;
						}
						if (iv_->IsProper() && !dd_->SaveInitialValue(pq_.get(), iv_.get())) return -2;
						iv_dumper_.reset();
					}
					if (impl_dumper_) {
						if (impl_->IsProper() && !dd_->SaveImplementation(pq_.get(), impl_.get())) return -2;
						impl_dumper_.reset();
					}
					if (ref_) {
						if (!dd_->SaveReference(pq_.get(), ref_.get(), extra_.get())) return -2;
						ref_.reset();
					}
					if (extra_dumper_) {
						if (extra_->IsProper() && !dd_->SaveExtraImplementation(pq_.get(), extra_.get())) return -2;
						extra_dumper_.reset();
					}
					extra_.reset();
					if (bridge_) {
						if (!dd_->SaveBridge(pq_.get(), bridge_.get())) return -2;
						bridge_.reset();
					}
					pq_.reset();
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadPqName() {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		xmlChar *name;
		if (!Trim(s, &name)) {
			xmlFree(s);
			return -2;
		}
		if (xmlStrlen(name) == 0) {
			cerr << "empty name of <physical-quantity>" << endl;
			xmlFree(s);
			return -2;
		}
		if (ContainNonGraphic(name)) {
			cerr << "<physical-quantity>'s name contains invalid character: \""
				 << s
				 << "\""
				 << endl;
			xmlFree(s);
			return -2;
		}

		pq_->set_name(xmlStrdup(name));
		xmlFree(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadDimension() {
		bool isEmpty = xmlTextReaderIsEmptyElement(text_reader_);
		char dimension_type[7];
		std::memset(dimension_type, 0, 7);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "scalar")) {
					pq_->set_col(1);
					pq_->set_row(1);
					snprintf(dimension_type, 7, "%s", (const char *)value);
				} else if (xmlStrEqual(value, BAD_CAST "vector")) {
					if (isEmpty) {
						cerr << "empty <dimension> of type vector" << endl;
						return -2;
					}
					snprintf(dimension_type, 7, "%s", (const char *)value);
				} else if (xmlStrEqual(value, BAD_CAST "matrix")) {
					if (isEmpty) {
						cerr << "empty <dimension> of type matrix" << endl;
						return -2;
					}
					snprintf(dimension_type, 7, "%s", (const char *)value);
				} else {
					cerr << "invalid type of <dimension>: " << value << endl;
					return -2;
				}
			}
		}
		if (dimension_type[0] == '\0') {
			cerr << "missing type of <dimension>" << endl;
			return -2;
		}
		i = xmlTextReaderRead(text_reader_);
		if (isEmpty)
			return i;
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "col")) {
					i = ReadCol();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "row")) {
					i = ReadRow();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "dimension")) {
					if (strcmp(dimension_type, "scalar") == 0) {
						if (pq_->col() * pq_->row() != 1) {
							cerr << "<dimension> of type scalar with col*row: "
								 << pq_->col() << '*' << pq_->row()
								 << endl;
							return -2;
						}
					} else {
						if (pq_->col() == 0) {
							cerr << "missing <col> in <dimension> of type " << dimension_type << endl;
							return -2;
						}
						if (pq_->row() == 0) {
							cerr << "missing <row> in <dimension> of type " << dimension_type << endl;
							return -2;
						}
						if (std::strcmp(dimension_type, "vector") == 0) {
							if (pq_->col() * pq_->row() != std::max(pq_->col(), pq_->row())) {
								cerr << "either <col> or <row> of <dimension> of type vector must be 1" << endl;
								return -2;
							}
						}
					}
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadCol() {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		xmlChar *col;
		if (!Trim(s, &col)) {
			xmlFree(s);
			return -2;
		}
		if (xmlStrlen(col) == 0) {
			cerr << "empty <col>" << endl;
			xmlFree(s);
			return -2;
		}
		if (xmlStrEqual(col, BAD_CAST "discretization-dependent")) {
			cerr << "unsupported discretization-dependent <col>" << endl;
			xmlFree(s);
			return -2;
		}

		int n = std::atoi((const char *)col);
		if (n <= 0) {
			cerr << "invalid value of <col>: " << col << endl;
			xmlFree(s);
			return -2;
		}
		pq_->set_col(n);
		xmlFree(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadRow() {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		xmlChar *row;
		if (!Trim(s, &row)) {
			xmlFree(s);
			return -2;
		}
		if (xmlStrlen(row) == 0) {
			cerr << "empty <row>" << endl;
			xmlFree(s);
			return -2;
		}
		if (xmlStrEqual(row, BAD_CAST "discretization-dependent")) {
			cerr << "unsupported discretization-dependent <row>" << endl;
			xmlFree(s);
			return -2;
		}

		int n = std::atoi((const char *)row);
		if (n <= 0) {
			cerr << "invalid value of <row>: " << row << endl;
			xmlFree(s);
			return -2;
		}
		pq_->set_row(n);
		xmlFree(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadMaxDelay() {
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		assert(s);

		xmlChar *max_delay;
		if (!Trim(s, &max_delay)) {
			xmlFree(s);
			return -2;
		}
		if (xmlStrlen(max_delay) == 0) {
			cerr << "empty <max-delay> of <physical-quantity> in module "
				 << module_->module_id()
				 << endl;
			xmlFree(s);
			// We do not want to raise the error for old models, so just skip it
			return xmlTextReaderNext(text_reader_);
		}
		if (ContainNonGraphic(max_delay)) {
			cerr << "<max-delay> contains invalid character: \""
				 << max_delay
				 << "\""
				 << endl;
			xmlFree(s);
			return -2;
		}

		pq_->set_max_delay(xmlStrdup(max_delay));
		xmlFree(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadValueTypeSet() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			cerr << "missing <value-type>" << endl;
			return -2;
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "value-type")) {
					i = ReadValueType();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <value-type-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "value-type-set")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadValueType() {
		xmlChar *unit_id;
		int i = ReadAttributeValue(BAD_CAST "unit-id", &unit_id);
		if (i <= 0) {
			cerr << "missing unit-id of <value-type>" << endl;
			return -2;
		}
		pq_->set_unit_id(unit_id); // TODO: check invalid values
		return xmlTextReaderRead(text_reader_);
	}

	int ReadInitialValue() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no children, we can ignore it safely.
			return xmlTextReaderRead(text_reader_);
		}
		iv_.reset(new InitialValue);
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "definition")) {
					iv_dumper_.reset(new phml::DefinitionDumper<InitialValue>(text_reader_,
																			  pq_->name(),
																			  iv_.get()));
					i = iv_dumper_->Read(0);
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child element of <initial-value>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "initial-value")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadImplementation() {
		impl_.reset(new Implementation);
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "definition")) {
					i = ReadDefinitionOfImplementation();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "extra-implementation")) {
					i = ReadExtraImplementation();
					if (i <= 0) return i;
					continue;
				} if (xmlStrEqual(local_name, BAD_CAST "bridge")) {
					i = ReadBridge();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "implementation")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadDefinitionOfImplementation() {
		std::unique_ptr<Definition> definition(new Definition);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				definition->set_type(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "sub-type")) {
				definition->set_sub_type(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "format")) {
				definition->set_format(xmlTextReaderValue(text_reader_));
			}
		}
		if (xmlStrEqual(definition->type(), BAD_CAST "assign")) {
			if (xmlStrEqual(definition->sub_type(), BAD_CAST "bridge")) {
				return xmlTextReaderNext(text_reader_);
			}
			if (xmlStrEqual(definition->format(), BAD_CAST "reference")) {
				// expect assigning a port or timeseries
				i = xmlTextReaderRead(text_reader_);
				while (i > 0) {
					int type = xmlTextReaderNodeType(text_reader_);
					if (type == XML_READER_TYPE_ELEMENT) {
						const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
						if (xmlStrEqual(local_name, BAD_CAST "reference")) {
							i = ReadReference();
							if (i <= 0) return i;
							continue;
						}
					} else if (type == XML_READER_TYPE_END_ELEMENT) {
						const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
						if (xmlStrEqual(local_name, BAD_CAST "definition")) {
							return xmlTextReaderRead(text_reader_);
						}
					}
					i = xmlTextReaderRead(text_reader_);
				}
				return i;
			}
		} else if (xmlStrEqual(definition->type(), BAD_CAST "graph")) {
			pq_->set_unit_id(xmlCharStrdup("0")); // unit: dimensionless
			phml::GraphReader<PQ, DatabaseDriver> graph_reader(pq_.get(), text_reader_, dd_.get());
			return graph_reader.Read();
		}
		// expect definition in MathML
		impl_dumper_.reset(new phml::DefinitionDumper<Implementation>(text_reader_,
																	  pq_->name(),
																	  impl_.get()));
		return impl_dumper_->Read(0);
	}

	int ReadReference() {
		ref_.reset(new Reference);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "port-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				int port_id = atoi((const char *)value);
				if (port_id <= 0) {
					cerr << "invalid port-id of <reference>: " << value << endl;
					return -2;
				}
				ref_->set_port_id(port_id);
			} else if (xmlStrEqual(local_name, BAD_CAST "timeseries-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				int timeseries_id = atoi((const char *)value);
				if (timeseries_id <= 0) {
					cerr << "invalid timeseries-id of <reference>: " << value << endl;
					return -2;
				}
				ref_->set_timeseries_id(timeseries_id);
			} else if (xmlStrEqual(local_name, BAD_CAST "element-id")) {
				ref_->set_element_id(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <reference>: " << local_name << endl;
				return -2;
			}
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadExtraImplementation() {
		extra_.reset(new ExtraImplementation);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				extra_->set_type(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "order")) {
				xmlChar *value = xmlTextReaderValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "before")) {
					extra_->set_order(value);
				} else if (xmlStrEqual(value, BAD_CAST "after")) {
					extra_->set_order(value);
				} else if (xmlStrlen(value) == 0) {
					xmlFree(value);
					/* ignore this with warning */
					cerr << "empty value of order of <extra-implementation> of "
						 << pq_->name()
						 << " in "
						 << module_->module_id()
						 << endl;
				} else {
					cerr << "unknown value of order: " << value << endl;
					xmlFree(value);
					return -2;
				}
			} else {
				cerr << "unknown attribute of <extra-implementation>: " << local_name << endl;
				return -2;
			}
		}
		if (!extra_->type()) {
			cerr << "missing type of <extra-implementation>" << endl;
			return -2;
		}
		if ( !xmlStrEqual(extra_->type(), BAD_CAST "instantaneous") &&
			 !xmlStrEqual(extra_->type(), BAD_CAST "multiple-input-assignment") ) {
			// TODO: other types of <extra-implementation>
			cerr << "unsupported type of <extra-implementation>: "
				 << extra_->type()
				 << endl;
			return -2;
		}

		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "definition")) {
					i = ReadDefinitionOfExtraImplementation();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child element of <extra-implementation>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "extra-implementation")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadDefinitionOfExtraImplementation() {
		std::unique_ptr<Definition> def(new Definition);
		int isEmpty = xmlTextReaderIsEmptyElement(text_reader_);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				def->set_type(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "sub-type")) {
				def->set_sub_type(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "format")) {
				def->set_format(xmlTextReaderValue(text_reader_));
			}
		}
		if (xmlStrEqual(extra_->type(), BAD_CAST "multiple-input-assignment")) {
			if (xmlStrEqual(def->type(), BAD_CAST "loop")) {
				cerr << "multiple-input-assignment's definition of type loop is not supported yet" << endl;
				return -2;
			}
			if (!xmlStrEqual(def->type(), BAD_CAST "reduction")) {
				cerr << "found <extra-implementation> of type multiple-input-assignment, but its definition's type is not reduction: "
					 << def->type() << endl;
				return -2;
			}
			extra_->set_definition(def.release());
			return xmlTextReaderNext(text_reader_);
		}
		assert(xmlStrEqual(extra_->type(), BAD_CAST "instantaneous"));
		if (isEmpty != 0) {
			cerr << "empty <definition> in <extra-implementation> of type instantaneous" << endl;
			return -2;
		}
		if (!extra_->order()) {
			cerr << "missing order of <extra-implementation>" << endl;
			return -2;
		}
		// expect definition in MathML
		extra_dumper_.reset(new phml::DefinitionDumper<ExtraImplementation>(text_reader_,
																			pq_->name(),
																			extra_.get()));
		return extra_dumper_->Read(0);
	}

	int ReadBridge() {
		bridge_.reset(new Bridge);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (!xmlStrEqual(value, BAD_CAST "sbml")) {
					cerr << "unknown type of <bridge>: " << value << endl;
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "sub-type")) {
				xmlChar *value = xmlTextReaderValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "species")) {
					bridge_->set_sub_type(value);
				} else if (xmlStrEqual(value, BAD_CAST "parameter")) {
					bridge_->set_sub_type(value);
				} else {
					cerr << "unknown sub-type of <bridge>: " << value << endl;
					xmlFree(value);
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "direction")) {
				xmlChar *value = xmlTextReaderValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "get")) {
					bridge_->set_direction(value);
				} else if (xmlStrEqual(value, BAD_CAST "set")) {
					bridge_->set_direction(value);
				} else {
					cerr << "unknown direction of <bridge>: " << value << endl;
					xmlFree(value);
					return -2;
				}
			} else {
				cerr << "unknown attribute of <bridge>: " << local_name << endl;
				return -2;
			}
		}
		if (!bridge_->sub_type()) {
			cerr << "missing sub-type of <bridge>" << endl;
			return -2;
		}
		if (!bridge_->direction()) {
			cerr << "missing direction of <bridge>" << endl;
			return -2;
		}

		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "connector")) {
					i = ReadConnector();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child element of <bridge>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "bridge")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadConnector() {
		assert(bridge_);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "parameter")) {
					if (!xmlStrEqual(bridge_->sub_type(), BAD_CAST "parameter")) {
						cerr << "mismatch between bridge's sub-type and connector's type" << endl;
						return -2;
					}
					continue;
				} else if (xmlStrEqual(value, BAD_CAST "species")) {
					if (!xmlStrEqual(bridge_->sub_type(), BAD_CAST "species")) {
						cerr << "mismatch between bridge's sub-type and connector's type" << endl;
						return -2;
					}
					continue;
				} else if (xmlStrEqual(value, BAD_CAST "reaction")) {
					// skip this connector
					return xmlTextReaderNext(text_reader_);
				} else {
					cerr << "unknown type of <connector>: " << local_name << endl;
					return -2;
				}
			} else {
				cerr << "unknown attribute of <connector>: " << local_name << endl;
				return -2;
			}
		}
		if (i < 0) return i;

		i = xmlTextReaderRead(text_reader_);
		if (i <= 0) return i;
		xmlChar *s = xmlTextReaderReadString(text_reader_);
		if (!s) {
			cerr << "missing body of <connector>" << endl;
			return -2;
		}
		bridge_->set_connector(s);
		return xmlTextReaderNext(text_reader_);
	}

	int ReadImport() const {
		std::unique_ptr<Import> import(new Import);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "type")) {
				xmlChar *value = xmlTextReaderValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "external")) {
					import->set_type(value);
				} else if (xmlStrEqual(value, BAD_CAST "internal")) {
					import->set_type(value);
				} else {
					cerr << "unknown type of <import>: " << value << endl;
					xmlFree(value);
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "format")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "sbml")) {
					import->set_format(Import::kSbml);
				} else {
					cerr << "unknown format of <import>: " << value << endl;
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "iref")) {
				import->set_iref(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "xref")) {
				import->set_xref(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "zref")) {
				import->set_zref(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <import>: " << local_name << endl;
				return -2;
			}
		}
		if (!import->type()) {
			cerr << "missing type of <import>" << endl;
			return -2;
		}
		if (import->format() == Import::kUnspecifiedFormat) {
			cerr << "missing format of <import>" << endl;
			return -2;
		}
		if (!dd_->SaveImport(module_.get(), import.get(), given_path_, model_path_)) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadTimeseries() {
		std::unique_ptr<Timeseries> ts(new Timeseries);
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "timeseries-id")) {
				ts->set_timeseries_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "name")) {
				// we can ignore the name
			} else if (xmlStrEqual(local_name, BAD_CAST "format")) {
				xmlChar *value = xmlTextReaderValue(text_reader_);
				if ( xmlStrEqual(value, BAD_CAST "csv") ||
					 xmlStrEqual(value, BAD_CAST "isd") ) {
					ts->set_format(value);
				} else {
					cerr << "unknown format of <timeseries>: " << value << endl;
					xmlFree(value);
					return -2;
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "iref")) {
				ts->set_iref(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "zref")) {
				ts->set_zref(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <timeseries>: " << local_name << endl;
				return -2;
			}
		}
		if (!ts->timeseries_id()) {
			cerr << "missing timeseries-id of <timeseries>" << endl;
			return -2;
		}
		if (!ts->format()) {
			cerr << "missing format of <timeseries>" << endl;
			return -2;
		}
		if (!ts->iref() && !ts->zref()) {
			cerr << "missing iref/zref of <timeseries>" << endl;
			return -2;
		}
		if (!dd_->SaveTimeseries(module_.get(), ts.get(), given_path_, model_path_)) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadTemplateSet() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no templates, so we have done
			return xmlTextReaderNext(text_reader_);
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "template")) {
					i = ReadTemplate();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <template-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "template-set")) {
					return xmlTextReaderNext(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadTemplate() {
		int i;
		std::unique_ptr<Template> t(new Template);
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "template-id")) {
				t->set_template_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "ref-module-id")) {
				t->set_ref_module_id(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <template>: " << local_name << endl;
				return -2;
			}
		}
		if (!t->template_id()) {
			cerr << "missing template-id of <template>" << endl;
			return -2;
		}
		if (!t->ref_module_id()) {
			cerr << "missing ref-module-id of <template>: " << t->template_id() << endl;
			return -2;
		}
		if (!dd_->SaveTemplate(t.get())) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadInstanceSet() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no instances, so we have done
			return xmlTextReaderRead(text_reader_);
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "instance")) {
					i = ReadInstance();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <instance-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "instance-set")) {
					return xmlTextReaderNext(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadInstance() {
		instance_.reset(new Instance);

		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "module-id")) {
				instance_->set_module_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "label")) {
				instance_->set_label(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <instance>: " << local_name << endl;
				return -2;
			}
		}
		if (!instance_->module_id()) {
			cerr << "missing module-id of <instance>" << endl;
			return -2;
		}
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "instance-of")) {
					i = ReadInstanceOf();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "target-module")) {
					i = ReadTargetModule();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "instance")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadInstanceOf() {
		assert(instance_);

		xmlChar *template_id;
		int i = ReadAttributeValue(BAD_CAST "template-id", &template_id);
		if (i <= 0) {
			cerr << "missing template-id of <instance-of>: " << instance_->module_id() << endl;
			return -2;
		}
		instance_->set_template_id(template_id);
		if (!dd_->SaveInstance(instance_.get())) return -2;
		return xmlTextReaderNext(text_reader_);
	}

	int ReadTargetModule() {
		xmlChar *module_id;
		int i = ReadAttributeValue(BAD_CAST "module-id", &module_id);
		if (i <= 0) {
			cerr << "missing module-id of <target-module>: " << instance_->module_id() << endl;
			return i;
		}
		std::unique_ptr<TargetModule> tm(new TargetModule(module_id));
		if (!dd_->SaveTargetModule(instance_.get(), tm.get())) return -2;
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "target-physical-quantity")) {
					i = ReadTargetPq(tm.get());
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "target-module")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadTargetPq(const TargetModule *tm) {
		assert(tm);

		int pq_id = 0;
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "physical-quantity-id")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				pq_id = atoi((const char *)value);
				if (pq_id <= 0) {
					cerr << "invalid physical-quantity-id of <target-physical-quantity>: " << tm->module_id() << endl;
					return -2;
				}
			}
		}
		if (pq_id <= 0) {
			cerr << "missing physical-quantity-id of <target-physical-quantity>: " << tm->module_id() << endl;
			return -2;
		}
		std::unique_ptr<TargetPq> tpq(new TargetPq(pq_id));
		std::unique_ptr<phml::DefinitionDumper<TargetPq> > tpq_dumper(new phml::DefinitionDumper<TargetPq>(text_reader_, tpq.get()));
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "definition")) {
					i = tpq_dumper->Read(0);
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "target-physical-quantity")) {
					if (!dd_->SaveTargetPq(tm, tpq.get())) return -2;
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadEdgeSet() {
		if (xmlTextReaderIsEmptyElement(text_reader_)) {
			// no edges, so we have done
			return xmlTextReaderRead(text_reader_);
		}
		int i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "edge")) {
					i = ReadEdge();
					if (i <= 0) return i;
					continue;
				} else {
					cerr << "unknown child of <edge-set>: " << local_name << endl;
					return -2;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "edge-set")) {
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadEdge() {
		edge_.reset();
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "edge-id")) {
				// ignored
			} else if (xmlStrEqual(local_name, BAD_CAST "type")) {
				const xmlChar *value = xmlTextReaderConstValue(text_reader_);
				if (xmlStrEqual(value, BAD_CAST "functional")) {
					edge_.reset(new Edge(Edge::kFunctional));
				} else if (xmlStrEqual(value, BAD_CAST "forwarding")) {
					edge_.reset(new Edge(Edge::kForwarding));
				} else {
					// let's skip this edge
					return xmlTextReaderNext(text_reader_);
				}
			} else if (xmlStrEqual(local_name, BAD_CAST "multiple")) {
				// ignored
			} else {
				cerr << "unknown attribute of <edge>: " << local_name << endl;
				return -2;
			}
		}
		if (!edge_) {
			cerr << "missing type of <edge>" << endl;
			return -2;
		}
		i = xmlTextReaderRead(text_reader_);
		while (i > 0) {
			int type = xmlTextReaderNodeType(text_reader_);
			if (type == XML_READER_TYPE_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "tail")) {
					i = ReadTail();
					if (i <= 0) return i;
					continue;
				} else if (xmlStrEqual(local_name, BAD_CAST "head")) {
					i = ReadHead();
					if (i <= 0) return i;
					continue;
				}
			} else if (type == XML_READER_TYPE_END_ELEMENT) {
				const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
				if (xmlStrEqual(local_name, BAD_CAST "edge")) {
					if (!dd_->SaveEdge(edge_.get())) return -2;
					return xmlTextReaderRead(text_reader_);
				}
			}
			i = xmlTextReaderRead(text_reader_);
		}
		return i;
	}

	int ReadTail() {
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "module-id")) {
				edge_->set_tail_module_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "port-id")) {
				edge_->set_tail_port_id(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <tail>: " << local_name << endl;
				return -2;
			}
		}
		if (!edge_->tail_module_id()) {
			cerr << "missing module-id of <tail>" << endl;
			return -2;
		}
		if (!edge_->tail_port_id()) {
			cerr << "missing port-id of <tail>" << endl;
			return -2;
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadHead() {
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, BAD_CAST "module-id")) {
				edge_->set_head_module_id(xmlTextReaderValue(text_reader_));
			} else if (xmlStrEqual(local_name, BAD_CAST "port-id")) {
				edge_->set_head_port_id(xmlTextReaderValue(text_reader_));
			} else {
				cerr << "unknown attribute of <head>: " << local_name << endl;
				return -2;
			}
		}
		if (!edge_->head_module_id()) {
			cerr << "missing module-id of <head>" << endl;
			return -2;
		}
		if (!edge_->head_port_id()) {
			cerr << "missing port-id of <head>" << endl;
			return -2;
		}
		return xmlTextReaderNext(text_reader_);
	}

	int ReadAttributeValue(const xmlChar *name, xmlChar **value)
	{
		int i;
		while ( (i = xmlTextReaderMoveToNextAttribute(text_reader_)) > 0) {
			if (xmlTextReaderIsNamespaceDecl(text_reader_)) continue;

			const xmlChar *local_name = xmlTextReaderConstLocalName(text_reader_);
			if (xmlStrEqual(local_name, name)) {
				*value = xmlTextReaderValue(text_reader_);
				return i;
			}
		}
		return i;
	}

	const boost::filesystem::path &given_path_;
	const boost::filesystem::path &model_path_;
	xmlTextReaderPtr &text_reader_;
	std::unique_ptr<DatabaseDriver> dd_;

	std::unique_ptr<Module> module_;
	std::unique_ptr<PQ> pq_;
	std::unique_ptr<InitialValue> iv_;
	std::unique_ptr<Implementation> impl_;
	std::unique_ptr<Reference> ref_;
	std::unique_ptr<ExtraImplementation> extra_;
	std::unique_ptr<Edge> edge_;
	std::unique_ptr<Bridge> bridge_;
	std::unique_ptr<Instance> instance_;

	std::unique_ptr<phml::DefinitionDumper<InitialValue> > iv_dumper_;
	std::unique_ptr<phml::DefinitionDumper<Implementation> > impl_dumper_;
	std::unique_ptr<phml::DefinitionDumper<ExtraImplementation> > extra_dumper_;
};

struct Schema {
	const char *name;
	const char *columns;
};

const Schema kModelTables[] = {
	{"edges", "(tail_module_id BLOB, tail_port_id INTEGER, head_module_id BLOB, head_port_id INTEGER)"},
	{"instances", "(module_id BLOB, template_id BLOB, label TEXT)"},
	{"ncs", "(rg_name TEXT, rg_seed TEXT, integration TEXT, sts_unit_id INTEGER, sts_value TEXT)"},
	{"tds", "(unit_id INTEGER, step TEXT, module_id BLOB)"},
	{"modules", "(module_id BLOB, type TEXT, name TEXT, capsulated_by BLOB, template_state TEXT)"},
	{"pqs", "(module_rowid INTEGER, type TEXT, pq_id INTEGER, unit_id INTEGER, name TEXT, ncols INTEGER, nrows INTEGER, max_delay TEXT)"},
	{"ivs", "(pq_rowid INTEGER, math TEXT)"},
	{"impls", "(pq_rowid INTEGER, math TEXT)"},
	{"nodes", "(pq_rowid INTEGER, node_id INTEGER, name TEXT)"},
	{"arcs", "(pq_rowid INTEGER, tail_node_id INTEGER, head_node_id INTEGER, type TEXT, math TEXT)"},
	{"refports", "(pq_rowid INTEGER, port_id INTEGER, reduction INTEGER)"},
	{"refts", "(pq_rowid INTEGER, timeseries_id INTEGER, element_id TEXT)"},
	{"extras", "(pq_rowid INTEGER, order_type TEXT, math TEXT)"},
	{"templates", "(template_id BLOB, ref_module_id BLOB)"},
	{"tms", "(instance_rowid INTEGER, module_id BLOB)"},
	{"tpqs", "(tm_rowid INTEGER, pq_id INTEGER, math TEXT)"},
	{"units", "(unit_id INTEGER, name TEXT)"},
	{"elements", "(unit_rowid INTEGER, unit_id INTEGER, exponent REAL, factor INTEGER, multiplier REAL, offset REAL)"},
	{"bridges", "(pq_rowid INTEGER, direction TEXT, sub_type TEXT, connector TEXT)"},
	{"imports", "(module_rowid INTEGER, type TEXT, ref TEXT)"},
	{"ports", "(module_rowid INTEGER, port_id INTEGER, direction TEXT, ref_pq_id INTEGER, multiple TEXT)"},
	{"timeseries", "(module_rowid INTEGER, timeseries_id INTEGER, format TEXT, ref TEXT)"}
};

const Schema kSubsequentTables[] = {
	{"private_variables", VARIABLES_SCHEMA}, // the same columns as public_variables
	{"trees", "(module_id BLOB, level INTEGER)"},
	{"scopes", "(uuid BLOB, space_id BLOB, label TEXT)"},
	{"journals", "(indent INTEGER, uuid BLOB)"},
	{"spans", "(tail_uuid BLOB, tail_port_id INTEGER, head_uuid BLOB, head_port_id INTEGER)"},
	{"reaches", "(output_uuid BLOB, output_id INTEGER, input_uuid BLOB, input_id INTEGER, reduction INTEGER)"},
	{"combined_values", "(uuid BLOB, math TEXT)"},
	{"combined_functions", "(uuid BLOB, math TEXT)"},
	{"combined_odes", "(uuid BLOB, math TEXT)"},
	{"tscs", "(uuid BLOB, math TEXT)"}
};

void CreateTablesOrDie(sqlite3 *db, const Schema *tables, size_t n)
{
	for (size_t i=0;i<n;i++) {
		const Schema &table = tables[i];
		if (!CreateTable(db, table.name, table.columns))
			exit(EXIT_FAILURE);
	}
}

#define CREATE_TABLES_OR_DIE(db, tables) \
	CreateTablesOrDie(db, tables, sizeof(tables)/sizeof(tables[0]))

struct View {
	const char *name;
	const char *rest_of_query;
};

const View kViews[] = {
	{"joins", "m.module_id AS module_id, i.module_id AS uuid, i.label AS label FROM instances AS i LEFT JOIN templates As t ON i.template_id = t.template_id LEFT JOIN modules AS m ON m.module_id = t.ref_module_id"},
	{"spaces", "module_id AS space_id, name FROM modules WHERE type = 'functional-unit'"},
	{"public_variables", "m.module_id AS space_id, p.type, p.pq_id AS id, p.name, u.name AS unit, p.ncols, p.nrows, p.max_delay AS capacity FROM pqs AS p LEFT JOIN modules AS m ON p.module_rowid = m.rowid LEFT JOIN units AS u ON p.unit_id = u.unit_id"},
	{"variables", "* FROM public_variables UNION ALL SELECT * FROM private_variables"},
	{"time_unit", "u.name FROM tds AS t JOIN units AS u ON t.unit_id = u.unit_id WHERE t.module_id IS NULL"},
	{"sv_eqs", "m.module_id, ltrim(i.math) FROM impls AS i LEFT JOIN pqs AS p ON i.pq_rowid = p.rowid LEFT JOIN modules AS m ON p.module_rowid = m.rowid WHERE p.type = 's' OR p.type = 'v'"},
	{"vx_eqs", "m.module_id, ltrim(i.math) FROM impls AS i LEFT JOIN pqs AS p ON i.pq_rowid = p.rowid LEFT JOIN modules AS m ON p.module_rowid = m.rowid WHERE p.type = 'v' OR p.type = 'x'"},
	{"iv_eqs", "m.module_id, ltrim(i.math) FROM ivs AS i LEFT JOIN pqs AS p ON i.pq_rowid = p.rowid LEFT JOIN modules AS m ON p.module_rowid = m.rowid"},
	{"input_ivs", "* FROM sv_eqs UNION ALL SELECT * FROM iv_eqs UNION ALL SELECT * FROM tscs UNION ALL SELECT * FROM combined_values UNION ALL SELECT * FROM combined_functions"},
	{"input_eqs", "* FROM vx_eqs UNION ALL SELECT * FROM tscs UNION ALL SELECT * FROM combined_functions UNION ALL SELECT * FROM combined_odes"},
	{"before_eqs", "m.module_id, ltrim(e.math) FROM extras AS e LEFT JOIN pqs AS p ON e.pq_rowid = p.rowid LEFT JOIN modules AS m ON p.module_rowid = m.rowid WHERE e.order_type = 'before'"},
	{"after_eqs", "m.module_id, ltrim(e.math) FROM extras AS e LEFT JOIN pqs AS p ON e.pq_rowid = p.rowid LEFT JOIN modules AS m ON p.module_rowid = m.rowid WHERE e.order_type = 'after'"}
};

void CreateViewsOrDie(sqlite3 *db, const View *views, size_t n)
{
	char buf[1024]; // long enough
	char *em;
	int e;

	for (size_t i=0;i<n;i++) {
		const View &view = views[i];
		sprintf(buf, "CREATE VIEW IF NOT EXISTS %s AS SELECT %s",
				view.name, view.rest_of_query);

		e = sqlite3_exec(db, buf, NULL, NULL, &em);
		if (e != SQLITE_OK) {
			cerr << "failed to create view " << view.name
				 << ": " << e
				 << ": " << em << endl;
			sqlite3_free(em);
			exit(EXIT_FAILURE);
		}
	}
}

#define CREATE_VIEWS_OR_DIE(db, views) \
	CreateViewsOrDie(db, views, sizeof(views)/sizeof(views[0]))

}

bool Read(sqlite3 *db)
{
	std::unique_ptr<char[]> given_filename(GetGivenFilename(db));
	boost::filesystem::path given_path(given_filename.get());
	std::unique_ptr<char[]> model_filename(GetModelFilename(db));
	boost::filesystem::path model_path(model_filename.get());

	// prepare database; create tables
	if (!BeginTransaction(db))
		return false;
	CREATE_TABLES_OR_DIE(db, kModelTables);

	// subsequent tables
	CREATE_TABLES_OR_DIE(db, kSubsequentTables);
	if (!CreateSprinkles(db))
		return false;
	if (!CreateTsfiles(db))
		return false;
	if (!CreateConfig(db))
		return false;

	// views
	CREATE_VIEWS_OR_DIE(db, kViews);
	if (!CreateLayout(db))
		return false;

	LIBXML_TEST_VERSION
	xmlInitParser();

	xmlTextReaderPtr text_reader = xmlReaderForFile(model_filename.get(), NULL, 0);
	if (!text_reader) {
		cerr << "could not read the input: " << model_filename.get() << endl;
		xmlCleanupParser();
		return false;
	}

	{
		std::unique_ptr<Reader> reader(new Reader(given_path, model_path, text_reader, db));
		if (reader->Read() < 0) return false;
	}

	{
		std::unique_ptr<CapsulatedByValidator> validator(new CapsulatedByValidator(db));
		if (!validator->Validate()) return false;
	}
	{
		DelayArgValidator dav(db);
		if (!dav.Validate()) return false;
	}

	{
		std::unique_ptr<phml::GraphIvRewriter> rewriter(new phml::GraphIvRewriter(db));
		if (!rewriter->Rewrite()) return false;
	}
	{
		static const char kImplSelectQuery[] = "SELECT rowid, pq_rowid, math FROM impls";
		static const char kImplUpdateQuery[] = "UPDATE impls SET math = ? WHERE rowid = ?";
		phml::GraphMathRewriter rewriter(db, kImplSelectQuery, kImplUpdateQuery);
		if (!rewriter.Rewrite()) return false;
	}
	{
		static const char kExtraSelectQuery[] = "SELECT rowid, pq_rowid, math FROM extras";
		static const char kExtraUpdateQuery[] = "UPDATE extras SET math = ? WHERE rowid = ?";
		phml::GraphMathRewriter rewriter(db, kExtraSelectQuery, kExtraUpdateQuery);
		if (!rewriter.Rewrite()) return false;
	}
	{
		static const char kTpqSelectQuery[] = \
			"SELECT tpqs.rowid, pqs.rowid, tpqs.math FROM tpqs"
			" LEFT JOIN tms ON tpqs.tm_rowid = tms.rowid"
			" LEFT JOIN modules ON tms.module_id = modules.module_id"
			" LEFT JOIN pqs ON modules.rowid = pqs.module_rowid"
			" WHERE tpqs.pq_id = pqs.pq_id";
		static const char kTpqUpdateQuery[] = "UPDATE tpqs SET math = ? WHERE rowid = ?";
		phml::GraphMathRewriter rewriter(db, kTpqSelectQuery, kTpqUpdateQuery);
		if (!rewriter.Rewrite()) return false;
	}
	{
		phml::TransitionForm form(db);
		if (!form()) return false;
	}

	{
		std::unique_ptr<TreeWriter> tw(new TreeWriter(db));
		if (!tw->Write()) return false;
	}

	if (!Branch(model_path, db)) return false;
	if (!Span(db)) return false;
	if (!Reach(db)) return false;
	if (!Sprinkle(db)) return false;

	if (!CommitTransaction(db))
		return false;

	if (!phml::CombineAll(db))
		return false;
	if (!ts::Tsc(db))
		return false;
	if (!db::Flow(db))
		return false;
	return true;
}

}
}
