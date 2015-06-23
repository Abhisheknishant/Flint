/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_EXEC_TASK_RUNNER_HH_
#define FLINT_EXEC_TASK_RUNNER_HH_

#include <memory>
#include "db/driver.h"
#include "db/read-only-driver.hh"
#include "task/config-reader.hh"
#include "sqlite3.h"

namespace exec {

class TaskRunner {
public:
	TaskRunner(int id, char *path);

	const char *dir() const {return dir_.get();}
	const char *layout() const {return layout_.get();}
	const char *generated_layout() const {return generated_layout_.get();}
	const char *init() const {return init_.get();}
	const task::ConfigReader &reader() const {return *reader_;}

	sqlite3 *GetDatabase();
	sqlite3 *GetModelDatabase();

	bool Run();

private:
	int id_;
	std::unique_ptr<char[]> path_;
	std::unique_ptr<char[]> dir_;
	std::unique_ptr<char[]> layout_;
	std::unique_ptr<char[]> generated_layout_;
	std::unique_ptr<char[]> init_;
	std::unique_ptr<db::Driver> db_driver_;
	std::unique_ptr<db::ReadOnlyDriver> modeldb_driver_;
	std::unique_ptr<task::ConfigReader> reader_;
};

}

#endif
