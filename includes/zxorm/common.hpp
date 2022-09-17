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
        Error(const char* const err, int sqlite_result=SQLITE_OK) : err(err), sqlite_result(sqlite_result) {}
        const char* err;
        int sqlite_result;

        operator std::string () const {
            std::ostringstream out;
            out << std::string(err);
            if (sqlite_result != SQLITE_OK) {
                const char* sqlErr = sqlite3_errstr(sqlite_result);
                out <<": " + std::string(sqlErr);
            }
            return out.str();
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

    template<bool... T>
    static constexpr bool AnyOf = (... || T);

    template<bool... T>
    static constexpr bool AllOf = (... && T);

    template<bool... T>
    struct IndexOfFirst {
        private:
        static constexpr int _impl() {
            int i = 0;
            int idx = -1;
            ([&]{
                i++;
                if (T) idx = i;
            }(), ...);

            return idx;
        }
        public:
        static constexpr int value = _impl();
    };
};
