#pragma once
#include "zxorm/common.hpp"
#include "zxorm/error.hpp"

#define ZXORM_GET_RESULT(lhs, rhs)                      \
        auto _temp_result##__LINE__ = (rhs);                   \
        if (_temp_result##__LINE__.is_error()) [[unlikely]]    \
            return _temp_result##__LINE__.error();             \
        lhs = _temp_result##__LINE__.value();

namespace zxorm {
    template <class T>
    struct [[nodiscard]] Result {
        protected:
        std::variant<Error, T> _result;

        // For debug builds we want to ensure that all potential
        // errors have been checked before the value can be recieved
#ifndef NDEBUG
        mutable bool _err_was_checked = false;
#endif
        void set_error_checked() const {
#ifndef NDEBUG
            _err_was_checked = true;
#endif
        }

        void enforce_error_checked() const {
#if !defined(NDEBUG) && defined(ZXORM_FORCE_ERR_CHECK)
            assert(_err_was_checked);
#endif
        }

        public:
        Result(const Result& r) = delete;
        Result& operator = (const Result&) = delete;

        Result& operator = (Result&& old) {
#ifndef NDEBUG
            _err_was_checked = old._err_was_checked;
            old._err_was_checked = true;
#endif
            _result = std::move(old._result);
            return *this;
        }

        Result(Result&& old) {
            *this = std::move(old);
        }

        Result(T r) : _result{std::move(r)} {}
        Result(Error r) : _result{std::move(r)} {}

        bool is_error() const
        {
            set_error_checked();
            return std::holds_alternative<Error>(_result);
        }

        operator bool () const
        {
            return !is_error();
        }

        operator const Error& () const {
            return std::get<Error>(_result);
        }

        const Error& error () const {
            return *this;
        }

        Error&& error () {
            return std::move(std::get<Error>(_result));
        }

        operator const T& () const {
            enforce_error_checked();
            return std::get<T>(_result);
        }

        operator T&& () {
            enforce_error_checked();
            return std::move(std::get<T>(_result));
        }

        const T& value() const {
            return *this;
        }

        T&& value() {
            return *this;
        }
    };

    template <typename T>
    struct OptionalResult : public Result<std::optional<T>> {
        using __base_t = Result<std::optional<T>>;
        OptionalResult(const std::nullopt_t) : __base_t{(std::optional<T>) std::nullopt} {}
        OptionalResult(T r) : __base_t{std::move(r)} {}
        OptionalResult(Error r) : __base_t{std::move(r)} {}
        OptionalResult(auto&&... args) : __base_t{T{std::forward<decltype(args)>(args)...}} {}

        OptionalResult (Result<T>&& result): __base_t{std::nullopt} {
            if (result.is_error()) {
                __base_t::_result = result.error();
            } else {
                __base_t::_result = std::move(result.value());
            }
        }

        bool has_value() {
            return not __base_t::is_error() && __base_t::value().has_value();
        }

        const T& value() const {
            return __base_t::value().value();
        }

        T&& value() {
            return __base_t::value().value();
        }
    };
};
