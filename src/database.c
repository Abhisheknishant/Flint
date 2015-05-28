/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "database.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db/query.h"
#include "sqlite3.h"

static int FindInputFile(sqlite3 *db,
						 const char column_name[],
						 char *input_file)
{
	static const int kMaxBytes = 1023;

	int r = 0;
	int e;
	sqlite3_stmt *stmt;
	char query[32]; /* long enough */
	sprintf(query, "SELECT %s FROM input", column_name);
	e = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to prepare statement: %d\n", e);
		goto bail0;
	}
	e = sqlite3_step(stmt);
	if (e == SQLITE_DONE) {
		fprintf(stderr, "input table is empty\n");
		goto bail1;
	}
	if (e != SQLITE_ROW) {
		fprintf(stderr, "failed to step statement: %d\n", e);
		goto bail1;
	}
	const void *f = sqlite3_column_blob(stmt, 0);
	if (!f) {
		fprintf(stderr, "%s is NULL", column_name);
		goto bail1;
	}
	int n = sqlite3_column_bytes(stmt, 0);
	if (n > kMaxBytes) {
		fprintf(stderr, "%s is too long: %d\n", column_name, n);
		goto bail1;
	}
	memcpy(input_file, f, n);
	input_file[n] = '\0';
	r = n;

 bail1:
	sqlite3_finalize(stmt);
 bail0:
	return r;
}

int FindGivenFile(sqlite3 *db, char *given_file)
{
	return FindInputFile(db, "given_file", given_file);
}

int FindModelFile(sqlite3 *db, char *model_file)
{
	return FindInputFile(db, "model_file", model_file);
}

int SaveGivenFile(sqlite3 *db, const char *given_file)
{
	int r = 0;
	int e;
	if (!CreateTable(db, "input", "(given_file BLOB, model_file BLOB)"))
		goto bail0;
	sqlite3_stmt *stmt;
	e = sqlite3_prepare_v2(db, "INSERT INTO input VALUES (?, ?)", -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to prepare statement: %d\n", e);
		goto bail0;
	}
	int len = (int)strlen(given_file);
	e = sqlite3_bind_blob(stmt, 1, given_file, len, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to bind parameter: %d\n", e);
		goto bail1;
	}
	e = sqlite3_bind_blob(stmt, 2, given_file, len, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to bind parameter: %d\n", e);
		goto bail1;
	}
	e = sqlite3_step(stmt);
	if (e != SQLITE_DONE) {
		fprintf(stderr, "failed to step statement: %d\n", e);
		goto bail1;
	}
	r = 1;

 bail1:
	sqlite3_finalize(stmt);
 bail0:
	return r;
}

int SaveModelFile(sqlite3 *db, const char *model_file)
{
	int r = 0;
	int e;
	sqlite3_stmt *stmt;
	e = sqlite3_prepare_v2(db, "UPDATE input SET model_file = ?", -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to prepare statement: %d\n", e);
		goto bail0;
	}
	int len = (int)strlen(model_file);
	e = sqlite3_bind_blob(stmt, 1, model_file, len, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to bind parameter: %d\n", e);
		goto bail1;
	}
	e = sqlite3_step(stmt);
	if (e != SQLITE_DONE) {
		fprintf(stderr, "failed to step statement: %d\n", e);
		goto bail1;
	}
	r = 1;

 bail1:
	sqlite3_finalize(stmt);
 bail0:
	return r;
}

int LoadExec(sqlite3 *db, char *sedml_file, char *phsp_file)
{
	static const int kMaxBytes = 1023;

	int r = 0;
	int e;
	sqlite3_stmt *stmt;
	e = sqlite3_prepare_v2(db, "SELECT sedml_file, phsp_file FROM input", -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to prepare statement: %d\n", e);
		goto bail0;
	}
	e = sqlite3_step(stmt);
	if (e == SQLITE_DONE) {
		fprintf(stderr, "input table is empty\n");
		goto bail1;
	}
	if (e != SQLITE_ROW) {
		fprintf(stderr, "failed to step statement: %d\n", e);
		goto bail1;
	}

	if (sedml_file) {
		const void *f = sqlite3_column_blob(stmt, 0);
		if (!f) {
			fprintf(stderr, "sedml file is NULL");
			goto bail1;
		}
		int n = sqlite3_column_bytes(stmt, 0);
		if (n > kMaxBytes) {
			fprintf(stderr, "sedml file name is too long: %d\n", n);
			goto bail1;
		}
		memcpy(sedml_file, f, n);
		sedml_file[n] = '\0';
	}
	if (phsp_file) {
		const void *f = sqlite3_column_blob(stmt, 1);
		if (!f) {
			fprintf(stderr, "phsp file is NULL");
			goto bail1;
		}
		int n = sqlite3_column_bytes(stmt, 1);
		if (n > kMaxBytes) {
			fprintf(stderr, "phsp file name is too long: %d\n", n);
			goto bail1;
		}
		memcpy(phsp_file, f, n);
		phsp_file[n] = '\0';
	}
	r = 1;

 bail1:
	sqlite3_finalize(stmt);
 bail0:
	return r;
}

int SaveExec(sqlite3 *db, const char *sedml_file, const char *phsp_file)
{
	int r = 0;
	int e;
	if (!CreateTable(db, "input", "(sedml_file BLOB, phsp_file BLOB)"))
		goto bail0;
	sqlite3_stmt *stmt;
	e = sqlite3_prepare_v2(db, "INSERT INTO input VALUES (?, ?)", -1, &stmt, NULL);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to prepare statement: %d\n", e);
		goto bail0;
	}
	int len1 = (int)strlen(sedml_file);
	e = sqlite3_bind_blob(stmt, 1, sedml_file, len1, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to bind parameter: %d\n", e);
		goto bail1;
	}
	int len2 = (int)strlen(phsp_file);
	e = sqlite3_bind_blob(stmt, 2, phsp_file, len2, SQLITE_STATIC);
	if (e != SQLITE_OK) {
		fprintf(stderr, "failed to bind parameter: %d\n", e);
		goto bail1;
	}
	e = sqlite3_step(stmt);
	if (e != SQLITE_DONE) {
		fprintf(stderr, "failed to step statement: %d\n", e);
		goto bail1;
	}
	r = 1;

 bail1:
	sqlite3_finalize(stmt);
 bail0:
	return r;
}
