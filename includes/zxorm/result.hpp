#pragma once
#include "zxorm/common.hpp"

namespace zxorm {
    template <class T>
    struct Result {
        protected:
        std::variant<Error, T> result;

        public:
        Result(const Result& r) = delete;
        Result& operator = (const Result&) = delete;
        Result(Result&& r) = default;
        Result& operator = (Result&&) = default;
        Result(T r) : result{std::move(r)} {}
        Result(Error r) : result{std::move(r)} {}

        bool is_error() const {
            return std::holds_alternative<Error>(result);
        }

        operator bool () const {
            return !is_error();
        }

        operator const Error& () const {
            return std::get<Error>(result);
        }

        const Error& error () const {
            return *this;
        }

        operator const T& () const {
            return std::get<T>(result);
        }

        operator T&& () {
            return std::move(std::get<T>(result));
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
