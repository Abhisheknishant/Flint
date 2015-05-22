/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sbml.hh"

#include "database.h"
#include "db/driver.h"

#define BOOST_TEST_MODULE test_load
#include "test.hh"

struct F {

	~F() {
		if (db_file) std::remove(db_file);
	}

	const char *db_file;
};

BOOST_FIXTURE_TEST_SUITE(test_load, F)

BOOST_AUTO_TEST_CASE(BIOMD0000000114) {
	db_file = "BIOMD0000000114.db";
	BOOST_CHECK_EQUAL(SaveGivenFile(db_file, TEST_MODELS("BIOMD0000000114.xml")), 1);
	BOOST_CHECK(flint::sbml::Parse(db_file));
	db::Driver driver(db_file);
	BOOST_CHECK(flint::sbml::Load(driver.db()));
}

BOOST_AUTO_TEST_CASE(BIOMD0000000152) {
	db_file = "BIOMD0000000152.db";
	BOOST_CHECK_EQUAL(SaveGivenFile(db_file, TEST_MODELS("BIOMD0000000152.xml")), 1);
	BOOST_CHECK(flint::sbml::Parse(db_file));
	db::Driver driver(db_file);
	BOOST_CHECK(flint::sbml::Load(driver.db()));
}

BOOST_AUTO_TEST_SUITE_END()
