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
    enum class log_level {
        Error = 0,
        Warning = 1,
        Info = 2,
        Debug = 3,
    };

    using Logger = std::function<void(log_level, const char*)>;

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

    struct [[nodiscard]] OptionalError {
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

        ~OptionalError() {
#ifndef NDEBUG
            assert(_err_was_checked);
#endif
        }
    };

    template<size_t N>
    struct FixedLengthString {
        constexpr FixedLengthString(const char (&str)[N]) {
            std::copy_n(str, N, value);
        }

        char value[N];
    };

    template<FixedLengthString delim, FixedLengthString...string>
    requires requires { (sizeof(string.value) + ...); }
    struct append_to_stream {
        friend std::ostream & operator<< (std::ostream &out, [[maybe_unused]] const append_to_stream& c)
        {
            std::array<std::string, sizeof...(string)> strings = {string.value...};
            std::copy(strings.begin(), strings.end(),
                    std::ostream_iterator<std::string>(out, delim.value));
            return out;
        }
    };

    template<bool... T>
    static constexpr bool any_of = (... || T);

    template<bool... T>
    static constexpr bool all_of = (... && T);

    template<bool... T>
    struct index_of_first {
        private:
        static constexpr int _impl() {
            constexpr std::array<bool, sizeof...(T)> a{T...};
            const auto it = std::find(a.begin(), a.end(), true);

            if (it == a.end()) return -1;
            return std::distance(a.begin(), it);
        }

        public:
        static constexpr int value = _impl();
    };

    template<typename T>
    struct remove_optional : std::type_identity<T> {};
    template<typename T>
    struct remove_optional<std::optional<T>> : std::type_identity<T> {};

    namespace traits {
        template<typename T>
        struct is_vector : std::false_type  { };
        template<typename T, typename A>
        struct is_vector<std::vector<T, A>> : std::true_type {};

        template<typename T>
        struct is_basic_string : std::false_type  { };
        template<typename CharT, typename Traits, typename Allocator>
        struct is_basic_string<std::basic_string<CharT, Traits, Allocator>> : std::true_type {};

        template<typename T>
        struct is_array : std::false_type  { };
        template<typename T, auto s>
        struct is_array<std::array<T, s>> : std::true_type {};

        template<typename T>
        struct is_optional : std::false_type  { };
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};

    }

    namespace ignore_qualifiers {
        template<typename T>
        static constexpr bool is_continuous_container() {
            using plain = remove_optional<std::remove_cvref_t<T>>::type;
            return traits::is_vector<plain>() || traits::is_basic_string<plain>() || traits::is_array<plain>();
        }

        template<typename T>
        static constexpr bool is_arithmetic() {
            using plain = remove_optional<std::remove_cvref_t<T>>::type;
            return std::is_arithmetic_v<plain>;
        }

        template<typename T>
        static constexpr bool is_optional() {
            using plain = std::remove_cvref_t<T>;
            return traits::is_optional<plain>();
        }

        template <typename T>
        static constexpr bool is_string() {
            using plain = remove_optional<std::remove_cvref_t<T>>::type;
            return traits::is_basic_string<plain>();
        }

        template <typename T>
        static constexpr bool is_floating_point() {
            using plain = remove_optional<std::remove_cvref_t<T>>::type;
            return std::is_floating_point_v<plain>;
        }
    };

    template<typename T>
    concept ContinuousContainer = ignore_qualifiers::is_continuous_container<T>();

    template<typename T>
    concept ArithmeticT = ignore_qualifiers::is_arithmetic<T>();

    template<typename T>
    concept OptionalT = ignore_qualifiers::is_optional<T>();

    // unqiue tuple https://stackoverflow.com/a/57528226
    namespace __unique_tuple_detail {
        template <typename T, typename... Ts>
        struct unique : std::type_identity<T> {};

        template <typename... Ts, typename U, typename... Us>
        struct unique<std::tuple<Ts...>, U, Us...>
            : std::conditional_t<(std::is_same_v<U, Ts> || ...)
                               , unique<std::tuple<Ts...>, Us...>
                               , unique<std::tuple<Ts..., U>, Us...>> {};
    };

    template <typename... Ts>
    using unique_tuple = typename __unique_tuple_detail::unique<std::tuple<>, Ts...>::type;
};
