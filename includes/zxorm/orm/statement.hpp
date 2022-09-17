#pragma once

#include "zxorm/common.hpp"
#include <optional>

namespace zxorm {
    template <class Connection, class... Table>
    class Statement {
        public:
        std::optional<Error> error = std::nullopt;

        Statement(Connection* conn, const std::string& query) : conn{conn} {
            conn->_logger(LogLevel::Debug, "Prepared statement");
            conn->_logger(LogLevel::Debug, query.c_str());

            int result = sqlite3_prepare_v2(conn->_db_handle, query.c_str(), query.size() + 1, &stmt, nullptr);
            if (result != SQLITE_OK) {
                const char* str = sqlite3_errstr(result);
                conn->_logger(LogLevel::Error, "Unable to initialize statment");
                conn->_logger(LogLevel::Error, str);
                error = Error("Unable to initialize statement", result);
                return;
            }

            parameterCount = sqlite3_bind_parameter_count(stmt);
        }

        ~Statement() {
            int result = sqlite3_finalize(stmt);
            if (result != SQLITE_OK) {
                const char* str = sqlite3_errstr(result);
                conn->_logger(LogLevel::Error, "Unable to finalize statment");
                conn->_logger(LogLevel::Error, str);
            }
        }

        private:
        friend Connection;
        Connection* conn;
        sqlite3_stmt* stmt;

        int parameterCount;
        int boundCount = 0;

        std::optional<Error>bind(size_t idx, void* data, size_t len) {
            int result = sqlite3_bind_blob(stmt, idx, data, len, nullptr);
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement");
                return Error("Unable to bind parameter to statment", result);
            }
            return std::nullopt;
        }

        std::optional<Error>bind(const char* paramName, void* data, size_t len) {
            int idx = sqlite3_bind_parameter_index(stmt, paramName);
            if (idx <= 0) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement, name not found");
                return Error("Unable to bind parameter to statment, name not found");
            }
            return bind(idx, data, len);
        }

        std::optional<Error> step() {
            if (boundCount != parameterCount) {
                return Error("Some parameters have not been bound");
            }

            int result = sqlite3_step(stmt);
            if (result != SQLITE_OK && result != SQLITE_DONE) {
                conn->_logger(LogLevel::Error, "Unable to execute statment");
                return Error("Unable to execute statment", result);
            }

            return std::nullopt;

        }
    };
};
