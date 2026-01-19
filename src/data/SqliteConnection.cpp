#include "beatlink/data/SQLiteConnection.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <stdexcept>

namespace beatlink::data {

// =============================================================================
// Implementation struct
// =============================================================================

struct SQLiteConnection::Impl {
    sqlite3* db = nullptr;

    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }
};

// =============================================================================
// SqlRow implementation
// =============================================================================

const SqlValue& SqlRow::operator[](const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) {
        static SqlValue nullValue = nullptr;
        return nullValue;
    }
    return values_[std::distance(columnNames_.begin(), it)];
}

bool SqlRow::isNull(size_t index) const {
    if (index >= values_.size()) return true;
    return std::holds_alternative<std::nullptr_t>(values_[index]);
}

bool SqlRow::isNull(const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) return true;
    return isNull(std::distance(columnNames_.begin(), it));
}

int64_t SqlRow::getInt(size_t index) const {
    if (index >= values_.size()) return 0;
    if (auto* val = std::get_if<int64_t>(&values_[index])) {
        return *val;
    }
    if (auto* val = std::get_if<double>(&values_[index])) {
        return static_cast<int64_t>(*val);
    }
    return 0;
}

int64_t SqlRow::getInt(const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) return 0;
    return getInt(std::distance(columnNames_.begin(), it));
}

double SqlRow::getDouble(size_t index) const {
    if (index >= values_.size()) return 0.0;
    if (auto* val = std::get_if<double>(&values_[index])) {
        return *val;
    }
    if (auto* val = std::get_if<int64_t>(&values_[index])) {
        return static_cast<double>(*val);
    }
    return 0.0;
}

double SqlRow::getDouble(const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) return 0.0;
    return getDouble(std::distance(columnNames_.begin(), it));
}

std::string SqlRow::getString(size_t index) const {
    if (index >= values_.size()) return "";
    if (auto* val = std::get_if<std::string>(&values_[index])) {
        return *val;
    }
    return "";
}

std::string SqlRow::getString(const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) return "";
    return getString(std::distance(columnNames_.begin(), it));
}

std::vector<uint8_t> SqlRow::getBlob(size_t index) const {
    if (index >= values_.size()) return {};
    if (auto* val = std::get_if<std::vector<uint8_t>>(&values_[index])) {
        return *val;
    }
    return {};
}

std::vector<uint8_t> SqlRow::getBlob(const std::string& name) const {
    auto it = std::find(columnNames_.begin(), columnNames_.end(), name);
    if (it == columnNames_.end()) return {};
    return getBlob(std::distance(columnNames_.begin(), it));
}

// =============================================================================
// SQLiteConnection implementation
// =============================================================================

SQLiteConnection::SQLiteConnection() : impl_(std::make_unique<Impl>()) {}

SQLiteConnection::SQLiteConnection(const std::string& path) : impl_(std::make_unique<Impl>()) {
    open(path);
}

SQLiteConnection::~SQLiteConnection() = default;

SQLiteConnection::SQLiteConnection(SQLiteConnection&& other) noexcept
    : impl_(std::move(other.impl_)), path_(std::move(other.path_)) {}

SQLiteConnection& SQLiteConnection::operator=(SQLiteConnection&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        path_ = std::move(other.path_);
    }
    return *this;
}

bool SQLiteConnection::open(const std::string& path) {
    close();
    impl_ = std::make_unique<Impl>();

    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        impl_->db = nullptr;
        return false;
    }

    path_ = path;
    return true;
}

bool SQLiteConnection::openEncrypted(const std::string& path, const std::string& key) {
#ifdef BEATLINK_HAS_SQLCIPHER
    close();
    impl_ = std::make_unique<Impl>();

    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        impl_->db = nullptr;
        return false;
    }

    // Set encryption key
    rc = sqlite3_key(impl_->db, key.c_str(), static_cast<int>(key.size()));
    if (rc != SQLITE_OK) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        return false;
    }

    // Verify the key by executing a simple query
    rc = sqlite3_exec(impl_->db, "SELECT count(*) FROM sqlite_master;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        return false;
    }

    path_ = path;
    return true;
#else
    (void)path;
    (void)key;
    return false;  // SQLCipher not available
#endif
}

bool SQLiteConnection::openMemory() {
    close();
    impl_ = std::make_unique<Impl>();

    int rc = sqlite3_open(":memory:", &impl_->db);
    if (rc != SQLITE_OK) {
        impl_->db = nullptr;
        return false;
    }

    path_ = ":memory:";
    return true;
}

void SQLiteConnection::close() {
    if (impl_ && impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
    path_.clear();
}

bool SQLiteConnection::isOpen() const {
    return impl_ && impl_->db != nullptr;
}

bool SQLiteConnection::execute(const std::string& sql) {
    if (!isOpen()) return false;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (errMsg) {
        sqlite3_free(errMsg);
    }

    return rc == SQLITE_OK;
}

SqlResult SQLiteConnection::query(const std::string& sql) {
    return query(sql, {});
}

SqlResult SQLiteConnection::query(const std::string& sql, const std::vector<SqlValue>& params) {
    SqlResult result;

    if (!isOpen()) return result;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return result;
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1);
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                sqlite3_bind_null(stmt, idx);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                sqlite3_bind_int64(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                sqlite3_bind_blob(stmt, idx, arg.data(),
                                  static_cast<int>(arg.size()), SQLITE_TRANSIENT);
            }
        }, params[i]);
    }

    // Get column names
    int colCount = sqlite3_column_count(stmt);
    std::vector<std::string> columnNames;
    columnNames.reserve(colCount);
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columnNames.emplace_back(name ? name : "");
    }
    result.setColumnNames(columnNames);

    // Fetch rows
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<SqlValue> values;
        values.reserve(colCount);

        for (int i = 0; i < colCount; ++i) {
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
                case SQLITE_NULL:
                    values.emplace_back(nullptr);
                    break;
                case SQLITE_INTEGER:
                    values.emplace_back(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    values.emplace_back(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, i));
                    values.emplace_back(std::string(text ? text : ""));
                    break;
                }
                case SQLITE_BLOB: {
                    const uint8_t* data = static_cast<const uint8_t*>(
                        sqlite3_column_blob(stmt, i));
                    int size = sqlite3_column_bytes(stmt, i);
                    values.emplace_back(std::vector<uint8_t>(data, data + size));
                    break;
                }
                default:
                    values.emplace_back(nullptr);
                    break;
            }
        }

        result.addRow(SqlRow(std::move(values), columnNames));
    }

    sqlite3_finalize(stmt);
    return result;
}

void SQLiteConnection::forEach(const std::string& sql,
                                std::function<bool(const SqlRow&)> callback) {
    forEach(sql, {}, callback);
}

void SQLiteConnection::forEach(const std::string& sql, const std::vector<SqlValue>& params,
                                std::function<bool(const SqlRow&)> callback) {
    if (!isOpen()) return;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return;

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1);
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                sqlite3_bind_null(stmt, idx);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                sqlite3_bind_int64(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                sqlite3_bind_blob(stmt, idx, arg.data(),
                                  static_cast<int>(arg.size()), SQLITE_TRANSIENT);
            }
        }, params[i]);
    }

    // Get column names
    int colCount = sqlite3_column_count(stmt);
    std::vector<std::string> columnNames;
    columnNames.reserve(colCount);
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columnNames.emplace_back(name ? name : "");
    }

    // Iterate rows
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<SqlValue> values;
        values.reserve(colCount);

        for (int i = 0; i < colCount; ++i) {
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
                case SQLITE_NULL:
                    values.emplace_back(nullptr);
                    break;
                case SQLITE_INTEGER:
                    values.emplace_back(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    values.emplace_back(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, i));
                    values.emplace_back(std::string(text ? text : ""));
                    break;
                }
                case SQLITE_BLOB: {
                    const uint8_t* data = static_cast<const uint8_t*>(
                        sqlite3_column_blob(stmt, i));
                    int size = sqlite3_column_bytes(stmt, i);
                    values.emplace_back(std::vector<uint8_t>(data, data + size));
                    break;
                }
                default:
                    values.emplace_back(nullptr);
                    break;
            }
        }

        if (!callback(SqlRow(std::move(values), columnNames))) {
            break;
        }
    }

    sqlite3_finalize(stmt);
}

std::string SQLiteConnection::lastError() const {
    if (!isOpen()) return "Database not open";
    return sqlite3_errmsg(impl_->db);
}

int64_t SQLiteConnection::lastInsertId() const {
    if (!isOpen()) return 0;
    return sqlite3_last_insert_rowid(impl_->db);
}

int SQLiteConnection::changesCount() const {
    if (!isOpen()) return 0;
    return sqlite3_changes(impl_->db);
}

bool SQLiteConnection::beginTransaction() {
    return execute("BEGIN TRANSACTION");
}

bool SQLiteConnection::commit() {
    return execute("COMMIT");
}

bool SQLiteConnection::rollback() {
    return execute("ROLLBACK");
}

bool SQLiteConnection::hasSqlCipher() {
#ifdef BEATLINK_HAS_SQLCIPHER
    return true;
#else
    return false;
#endif
}

} // namespace beatlink::data
