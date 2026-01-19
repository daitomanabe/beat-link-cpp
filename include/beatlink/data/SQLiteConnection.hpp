#pragma once

/**
 * SqliteConnection - SQLite database wrapper
 *
 * Provides a C++ wrapper for SQLite3, used by OpusProvider
 * to access Device Library Plus databases.
 *
 * Optionally supports SQLCipher for encrypted databases
 * when BEATLINK_HAS_SQLCIPHER is defined.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace beatlink::data {

/**
 * Represents a value in a SQLite result row
 */
using SqlValue = std::variant<std::nullptr_t, int64_t, double, std::string, std::vector<uint8_t>>;

/**
 * A single row from a query result
 */
class SqlRow {
public:
    SqlRow() = default;
    explicit SqlRow(std::vector<SqlValue> values, std::vector<std::string> columnNames)
        : values_(std::move(values)), columnNames_(std::move(columnNames)) {}

    /**
     * Get value by column index
     */
    const SqlValue& operator[](size_t index) const { return values_.at(index); }

    /**
     * Get value by column name
     */
    const SqlValue& operator[](const std::string& name) const;

    /**
     * Get number of columns
     */
    size_t size() const { return values_.size(); }

    /**
     * Check if column is NULL
     */
    bool isNull(size_t index) const;
    bool isNull(const std::string& name) const;

    /**
     * Get integer value
     */
    int64_t getInt(size_t index) const;
    int64_t getInt(const std::string& name) const;

    /**
     * Get double value
     */
    double getDouble(size_t index) const;
    double getDouble(const std::string& name) const;

    /**
     * Get string value
     */
    std::string getString(size_t index) const;
    std::string getString(const std::string& name) const;

    /**
     * Get blob value
     */
    std::vector<uint8_t> getBlob(size_t index) const;
    std::vector<uint8_t> getBlob(const std::string& name) const;

    /**
     * Get column names
     */
    const std::vector<std::string>& columnNames() const { return columnNames_; }

private:
    std::vector<SqlValue> values_;
    std::vector<std::string> columnNames_;
};

/**
 * Query result containing multiple rows
 */
class SqlResult {
public:
    SqlResult() = default;

    void addRow(SqlRow row) { rows_.push_back(std::move(row)); }
    void setColumnNames(std::vector<std::string> names) { columnNames_ = std::move(names); }

    /**
     * Get row by index
     */
    const SqlRow& operator[](size_t index) const { return rows_.at(index); }

    /**
     * Get number of rows
     */
    size_t rowCount() const { return rows_.size(); }

    /**
     * Check if result is empty
     */
    bool empty() const { return rows_.empty(); }

    /**
     * Get column names
     */
    const std::vector<std::string>& columnNames() const { return columnNames_; }

    /**
     * Iterate over rows
     */
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

private:
    std::vector<SqlRow> rows_;
    std::vector<std::string> columnNames_;
};

/**
 * SQLite database connection
 */
class SQLiteConnection {
public:
    SQLiteConnection();
    explicit SQLiteConnection(const std::string& path);
    ~SQLiteConnection();

    // Non-copyable, movable
    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;
    SQLiteConnection(SQLiteConnection&& other) noexcept;
    SQLiteConnection& operator=(SQLiteConnection&& other) noexcept;

    /**
     * Open a database file
     * @param path Path to the database file
     * @return true if successful
     */
    bool open(const std::string& path);

    /**
     * Open an encrypted database (requires SQLCipher)
     * @param path Path to the database file
     * @param key Encryption key
     * @return true if successful
     */
    bool openEncrypted(const std::string& path, const std::string& key);

    /**
     * Open an in-memory database
     * @return true if successful
     */
    bool openMemory();

    /**
     * Close the database
     */
    void close();

    /**
     * Check if database is open
     */
    bool isOpen() const;

    /**
     * Execute a SQL statement (no result expected)
     * @param sql SQL statement
     * @return true if successful
     */
    bool execute(const std::string& sql);

    /**
     * Execute a query and return results
     * @param sql SQL query
     * @return Query results
     */
    SqlResult query(const std::string& sql);

    /**
     * Execute a query with parameters
     * @param sql SQL query with ? placeholders
     * @param params Parameter values
     * @return Query results
     */
    SqlResult query(const std::string& sql, const std::vector<SqlValue>& params);

    /**
     * Execute a query and call callback for each row
     * @param sql SQL query
     * @param callback Called for each row, return false to stop
     */
    void forEach(const std::string& sql,
                 std::function<bool(const SqlRow&)> callback);

    /**
     * Execute a query with parameters and call callback for each row
     */
    void forEach(const std::string& sql, const std::vector<SqlValue>& params,
                 std::function<bool(const SqlRow&)> callback);

    /**
     * Get the last error message
     */
    std::string lastError() const;

    /**
     * Get the last insert row ID
     */
    int64_t lastInsertId() const;

    /**
     * Get the number of rows affected by the last statement
     */
    int changesCount() const;

    /**
     * Begin a transaction
     */
    bool beginTransaction();

    /**
     * Commit the current transaction
     */
    bool commit();

    /**
     * Rollback the current transaction
     */
    bool rollback();

    /**
     * Check if SQLCipher is available
     */
    static bool hasSqlCipher();

    /**
     * Get database path (for compatibility)
     */
    const std::string& getSourcePath() const { return path_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string path_;
};

} // namespace beatlink::data
