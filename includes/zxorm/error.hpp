/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include "zxorm/common.hpp"
#include <cassert>
#include <exception>

namespace zxorm {
    inline bool is_constraint_error(int result) {
        return (result & 0xff) == SQLITE_CONSTRAINT;
    }

    class Error : std::exception {
    protected:
        std::string _message;
        int _sqlite_result = SQLITE_OK;
    public:
        const char* what() const noexcept override {
            return _message.c_str();
        }

        operator std::string_view () const noexcept {
            return _message;
        }

        int sqlite_errcode() const noexcept {
            return _sqlite_result;
        }

    protected:
        Error() = default;
        explicit Error(const std::string& message) : _message(message) {}

        explicit Error(const char* const err, sqlite3* handle) {
            _sqlite_result = sqlite3_extended_errcode(handle);

            std::ostringstream os;
            os << err;
            if (_sqlite_result != SQLITE_OK) {
                os << ": ["
                    << sqlite3_errstr(_sqlite_result)
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

    class SQLConstraintError : public Error {
    protected:
        SQLConstraintError() = default;
    public:
        explicit SQLConstraintError(const char* const err, sqlite3* handle) : Error(err, handle) {}
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
