#pragma once
#include "zxorm/common.hpp"
#include <cassert>
#include <exception>

namespace zxorm {
    class Error : std::exception {
    protected:
        std::string _message;
    public:
        const char* what() const noexcept override {
            return _message.c_str();
        }

        operator std::string_view () const noexcept {
            return _message;
        }

    protected:
        Error() = default;
        explicit Error(const std::string& message) : _message(message) {}

        explicit Error(const char* const err, sqlite3* handle) {
            int sqlite_result = SQLITE_OK;

            sqlite_result = sqlite3_errcode(handle);

            std::ostringstream os;
            os << err;
            if (sqlite_result != SQLITE_OK) {
                os << ": ["
                    << sqlite3_errstr(sqlite_result)
                    << "] "
                    << sqlite3_errmsg(handle);
            }

            _message = os.str();
        }
    };

    class SQLExecutionError : public Error {
    protected:
        SQLExecutionError() = default;
    public:
        explicit SQLExecutionError(const char* const err, sqlite3* handle) : Error(err, handle) {}
    };

    class ConnectionError : public Error {
    public:
        explicit ConnectionError(const std::string& message) : Error(message) {}
        explicit ConnectionError(const char* const err, sqlite3* handle) : Error(err, handle) {}
    };

    class InternalError : public Error {
    protected:
        InternalError() = default;
    public:
        explicit InternalError(const std::string& message) : Error(message) {}
        explicit InternalError(const char* const err, sqlite3* handle) : Error(err, handle) {}
    };

};
