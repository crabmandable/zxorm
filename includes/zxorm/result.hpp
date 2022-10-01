#pragma once
#include "zxorm/common.hpp"

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
#ifndef NDEBUG
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
    };
};
