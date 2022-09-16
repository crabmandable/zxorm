#pragma once

#include <sqlite3.h>
#include "zxorm/common.hpp"
#include <functional>
#include <sstream>
#include <memory>
#include <optional>

namespace zxorm {
    template <class... Table>
    requires(sizeof...(Table) > 0)
    class Connection {
    public:
        static Maybe<Connection<Table...>> create(const char* fileName, int flags = 0, const char* zVfs = nullptr, Logger logger = nullptr) {
            if (!flags) {
                flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            }

            if (logger) {
                std::ostringstream oss;
                oss << "Opening sqlite connection with flags: " << flags;
                logger(LogLevel::Debug, oss.str().c_str());
            }

            sqlite3* db_handle = nullptr;
            int result = sqlite3_open_v2(fileName, &db_handle, flags, zVfs);
            if (result != SQLITE_OK || !db_handle) {
                const char* str = sqlite3_errstr(result);
                if (logger) {
                    logger(LogLevel::Error, "Unable to open sqlite connection");
                    logger(LogLevel::Error, str);
                }
                return Error("Unable to open sqlite connection", result);
            }

            return Connection(db_handle, logger);
        }

        ~Connection() {
            int result = sqlite3_close_v2(_db_handle);
            if (result != SQLITE_OK) {
                const char* str = sqlite3_errstr(result);
                _logger(LogLevel::Error, "Unable to destruct connection");
                _logger(LogLevel::Error, str);
            }
        }

        Connection(Connection&& old) {
            _logger = old._logger;
            old._logger = [](auto...) {};
            _db_handle = old._db_handle;
            old._db_handle = nullptr;
        };

        Connection& operator =(Connection&&) = default;

        std::optional<Error> createTables(bool ifNotExist = true) {
            std::optional<Error> error = std::nullopt;
            // TODO make this a transaction
            ([&] {
                if (error) return;
                std::string createQ = Table::createTableQuery(ifNotExist);
                Statement s = {this, createQ};
                error = s.error;
                if (error) return;
                error = s.step();
            }(), ...);

            return error;
        }

    private:
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;
        Connection(sqlite3* db_handle, Logger logger) : _db_handle(db_handle) {
            if (logger) _logger = logger;
            else _logger = [](auto...) {};
        }

        struct Statement {
            Connection<Table...>* conn;
            sqlite3_stmt* stmt;
            std::optional<Error> error = std::nullopt;

            Statement(Connection<Table...>* conn, const std::string& query) : conn{conn} {
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

            std::optional<Error> step() {
                int result = sqlite3_step(stmt);
                if (result != SQLITE_OK && result != SQLITE_DONE) {
                    conn->_logger(LogLevel::Error, "Unable to execute statment");
                    return Error("Unable to execute statment", result);
                }

                return std::nullopt;

            }
        };

        sqlite3* _db_handle;
        Logger _logger = nullptr;
    };
};
