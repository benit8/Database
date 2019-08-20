/*
** SQLite3 encapsulation, August 2019
** Benoît LORMEAU <b@benito.io>
*/

/// https://www.sqlite.org/cintro.html

#pragma once

////////////////////////////////////////////////////////////////////////////////

namespace SQL {
	class Value;
	class Statement;
	class Database;
}

////////////////////////////////////////////////////////////////////////////////

#include <sqlite3.h>
#include <stdint.h>
#include <cassert>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

////////////////////////////////////////////////////////////////////////////////

namespace SQL
{

typedef struct { const void *data; size_t size; } Blob;
typedef std::unordered_map<std::string, Value> Row;

////////////////////////////////////////////////////////////////////////////////

class Value
{
public:
	Value(sqlite3_value *value = NULL)
	: m_value(sqlite3_value_dup(value))
	{
		assert(m_value != NULL);
	}

	Value(const Value &other)
	: m_value(sqlite3_value_dup(other.m_value))
	{
		assert(m_value != NULL);
	}

	~Value()
	{
		sqlite3_value_free(m_value);
	}


	int integer() const
	{
		return sqlite3_value_int(m_value);
	}

	int64_t bigInteger() const
	{
		return sqlite3_value_int64(m_value);
	}

	double real() const
	{
		return sqlite3_value_double(m_value);
	}

	void *pointer() const
	{
		return sqlite3_value_pointer(m_value, NULL);
	}

	std::string text() const
	{
		return reinterpret_cast<const char *>(sqlite3_value_text(m_value));
	}

	Blob blob() const
	{
		return { sqlite3_value_blob(m_value), size() };
	}

	std::size_t size() const
	{
		return sqlite3_value_bytes(m_value);
	}

private:
	sqlite3_value *m_value;
};

////////////////////////////////////////////////////////////////////////////////

class Statement
{
private:
	Statement(sqlite3_stmt *stmt, sqlite3 *dbRef)
	: m_stmt(stmt)
	, m_db(dbRef)
	{} friend class Database;

public:
	~Statement()
	{
		if (m_stmt != NULL)
			sqlite3_finalize(m_stmt);
	}

	void reset()
	{
		if (m_stmt != NULL)
			sqlite3_reset(m_stmt);
	}

	bool bind(std::size_t i, const Blob &data) {
		return sqlite3_bind_blob(m_stmt, i, data.data, data.size, SQLITE_TRANSIENT);
	}

	bool bind(std::size_t i, double data) {
		return sqlite3_bind_double(m_stmt, i, data);
	}

	bool bind(std::size_t i, int data) {
		return sqlite3_bind_int(m_stmt, i, data);
	}

	bool bind(std::size_t i, int64_t data) {
		return sqlite3_bind_int64(m_stmt, i, data);
	}

	bool bind(std::size_t i, nullptr_t /*data*/) {
		return sqlite3_bind_null(m_stmt, i);
	}

	bool bind(std::size_t i, const std::string &data) {
		return sqlite3_bind_text(m_stmt, i, data.c_str(), data.size(), SQLITE_TRANSIENT);
	}

	template <typename... Args>
	bool execute(Args... args)
	{
		if (m_stmt == NULL)
			return false;

		auto tuple = std::tuple<Args...>(args...);
		execute_impl(tuple, std::index_sequence_for<Args...>{});

		int s = sqlite3_step(m_stmt);
		if (s != SQLITE_DONE)
			std::cerr << "Statement execution failed: " << sqlite3_errmsg(m_db) << std::endl;
		return s == SQLITE_DONE;
	}

	bool fetch(Row &row)
	{
		if (m_stmt == NULL)
			return false;

		row.clear();

		if (sqlite3_step(m_stmt) != SQLITE_ROW)
			return false;

		for (size_t i = 0; i < colCount(); ++i)
			row.emplace(colName(i), colValue(i));

		return true;
	}

	std::vector<Row> fetchAll()
	{
		if (m_stmt == NULL)
			return {};

		Row row;
		std::vector<Row> rows;
		while (fetch(row))
			rows.push_back(row);

		return rows;
	}

	std::size_t colCount() const
	{
		return sqlite3_column_count(m_stmt);
	}

	std::string colName(std::size_t i) const
	{
		return sqlite3_column_name(m_stmt, i);
	}

	Value colValue(std::size_t i) const
	{
		return sqlite3_column_value(m_stmt, i);
	}

	std::size_t colSize(std::size_t i) const
	{
		return sqlite3_column_bytes(m_stmt, i);
	}

	std::string queryString() const
	{
		return sqlite3_sql(m_stmt);
	}

	operator bool() const {
		return m_stmt != NULL;
	}

private:
	template <typename... Args, size_t... I>
	void execute_impl(const std::tuple<Args...> &tuple, std::index_sequence<I...>) {
		(void)(std::initializer_list<int> { bind(I + 1, std::get<I>(tuple))... });
	}

private:
	sqlite3_stmt *m_stmt;
	sqlite3 *m_db;
};

////////////////////////////////////////////////////////////////////////////////

class Database
{
public:
	Database(const std::string &DBFilename)
	: m_DB(NULL)
	, m_DBfilename(DBFilename)
	{
		if (sqlite3_open(DBFilename.c_str(), &m_DB) != SQLITE_OK) {
			throw std::runtime_error(sqlite3_errmsg(m_DB));
		}
	}

	~Database()
	{
		sqlite3_close(m_DB);
	}


	bool exec(const std::string &queryString)
	{
		int r = sqlite3_exec(m_DB, queryString.c_str(), NULL, NULL, NULL);
		if (r != SQLITE_OK)
			std::cerr << "SQL exec() failed: " << sqlite3_errmsg(m_DB) << std::endl;
		return r == SQLITE_OK;
	}

	/// Apply a callback to each results of the query
	bool query(const std::string &queryString, std::function<bool(Row &)> callback)
	{
		Statement stmt = prepare(queryString);
		if (!stmt)
			return false;

		Row row;
		while (stmt.fetch(row)) {
			if (!callback(row))
				return false;
		}

		return true;
	}

	Statement prepare(const std::string &queryString)
	{
		sqlite3_stmt *stmt = NULL;

		int r = sqlite3_prepare_v2(m_DB, queryString.c_str(), queryString.size(), &stmt, NULL);
		if (r != SQLITE_OK) {
			std::cerr << "Database prepare failed: " << sqlite3_errmsg(m_DB) << std::endl;
		}

		return Statement(stmt, m_DB);
	}

	std::int64_t lastInsertId() const
	{
		return sqlite3_last_insert_rowid(m_DB);
	}

private:
	sqlite3 *m_DB;
	std::string m_DBfilename;
};

////////////////////////////////////////////////////////////////////////////////

}