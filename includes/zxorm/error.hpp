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
        Error() = default;

        Error(const char* const err, sqlite3* handle=nullptr) {
            if (handle)
                sqlite_result = sqlite3_errcode(handle);

            std::ostringstream os;
            os << err;
            if (sqlite_result != SQLITE_OK) {
                os << ": ["
                    << sqlite3_errstr(sqlite_result)
                    << "] "
                    << sqlite3_errmsg(handle);
            }

            err_str = os.str();
        }

        std::string err_str;
        int sqlite_result = SQLITE_OK;

        friend std::ostream & operator<< (std::ostream &out, const Error& e) {
            out << std::string(e.err_str);
            return out;
        }

        operator std::string () const {
            return err_str;
        }

        operator std::string_view () const {
            return err_str;
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
