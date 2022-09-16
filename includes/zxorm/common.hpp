#pragma once
#include <sqlite3.h>
#include <type_traits>
#include <variant>
#include <functional>
#include <string_view>
#include <string>
#include <sstream>
#include <iterator>

namespace zxorm {
    enum class LogLevel {
        Error = 0,
        Warning = 1,
        Info = 2,
        Debug = 3,
    };

    using Logger = std::function<void(LogLevel, const char*)>;

    struct Error {
        Error(const char* const err, int sqlite_result) : err(err), sqlite_result(sqlite_result) {}
        const char* err;
        int sqlite_result;

        operator std::string () const {
            const char* sqlErr = sqlite3_errstr(sqlite_result);
            std::string out = std::string(err) + ": " + std::string(sqlErr);
            return out;
        }
    };

    template <class T>
    using Maybe = std::variant<Error, T>;

    template<size_t N>
    struct FixedLengthString {
        constexpr FixedLengthString(const char (&str)[N]) {
            std::copy_n(str, N, value);
        }

        char value[N];
    };


    template<typename... T>
    auto appendToStringStream(std::ostringstream& ss, const char * delim) {
        std::array<std::string, sizeof...(T)> strings = {T::to_string()...};
        std::copy(strings.begin(), strings.end(),
                std::ostream_iterator<std::string>(ss, delim));
    }

    template<FixedLengthString...string>
    requires requires { (sizeof(string.value) + ...); }
    void appendToStringStream(std::ostringstream& ss, const char * delim) {
        std::array<std::string, sizeof...(string)> strings = {string.value...};
        std::copy(strings.begin(), strings.end(),
                std::ostream_iterator<std::string>(ss, delim));
    }
};
