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
            }
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
        std::optional<Error> step() {
            int result = sqlite3_step(stmt);
            if (result != SQLITE_OK && result != SQLITE_DONE) {
                conn->_logger(LogLevel::Error, "Unable to execute statment");
                return Error("Unable to execute statment", result);
            }

            return std::nullopt;

        }
    };
};
