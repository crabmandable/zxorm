#pragma once
#include "zxorm/common.hpp"
#include <cassert>

#define ZXORM_TRY(expression)                    \
    {                                            \
        auto _temp_maybe_err = (expression);     \
        if (_temp_maybe_err) [[unlikely]]        \
            return _temp_maybe_err.value();      \
    }

namespace zxorm {
    struct Error {
        Error(const char* const err, int sqlite_result=SQLITE_OK) : err(err), sqlite_result(sqlite_result) {}
        const char* err;
        int sqlite_result;

        friend std::ostream & operator<< (std::ostream &out, const Error& e) {
            out << std::string(e.err);
            if (e.sqlite_result != SQLITE_OK) {
                const char* sql_err = sqlite3_errstr(e.sqlite_result);
                out <<": " + std::string(sql_err);
            }
            return out;
        }

        operator std::string () const {
            std::ostringstream out;
            out << *this;
            return out.str();
        }
    };

    struct OptionalError {
    private:
        std::optional<Error> _err;

    public:
        OptionalError(auto value) : _err{value} {}
        OptionalError() = default;
        OptionalError(const OptionalError&) = delete;
        OptionalError& operator=(const OptionalError&) = delete;

        OptionalError& operator=(OptionalError&& old) {
#ifndef NDEBUG
            _err_was_checked = old._err_was_checked;
            old._err_was_checked = true;
#endif
            _err = old._err;
            return *this;
        }

        OptionalError(OptionalError&& old) {
            *this = std::move(old);
        }

#ifndef NDEBUG
        mutable bool _err_was_checked = false;
#endif
        void set_err_checked() const {
#ifndef NDEBUG
            _err_was_checked = true;
#endif
        }

        bool has_value() const {
            set_err_checked();
            return _err.has_value();
        }

        operator bool () const {
            set_err_checked();
            return has_value();
        }

        auto value() const {
            set_err_checked();
            return _err.value();
        }

        bool is_error() {
            return has_value();
        }

        auto error() {
            return value();
        }

        ~OptionalError() {
#if !defined(NDEBUG) && defined(ZXORM_FORCE_ERR_CHECK)
            assert(_err_was_checked);
#endif
        }
    };
};
