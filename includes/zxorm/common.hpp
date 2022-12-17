#pragma once
#include <sqlite3.h>
#include <type_traits>
#include <variant>
#include <functional>
#include <string_view>
#include <string>
#include <sstream>
#include <iterator>
#include <optional>

namespace zxorm {
    enum class log_level {
        Error = 0,
        Warning = 1,
        Info = 2,
        Debug = 3,
    };

    using Logger = std::function<void(log_level, const std::string_view&)>;

    template<size_t N>
    struct FixedLengthString {
        constexpr FixedLengthString(const char (&str)[N]) {
            std::copy_n(str, N, value);
        }

        template<typename OtherFLStr>
        consteval bool operator==(const OtherFLStr& other) const {
            if (other.size != size) return false;
            for (size_t i = 0; i < N; i++) {
                if (value[i] != other.value[i]) return false;
            }
            return true;
        }

        char value[N];
        static constexpr size_t size = N;
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
            using plain = typename remove_optional<std::remove_cvref_t<T>>::type;
            return traits::is_vector<plain>() || traits::is_basic_string<plain>() || traits::is_array<plain>();
        }

        template<typename T>
        static constexpr bool is_arithmetic() {
            using plain = typename remove_optional<std::remove_cvref_t<T>>::type;
            return std::is_arithmetic_v<plain>;
        }

        template<typename T>
        static constexpr bool is_optional() {
            using plain = std::remove_cvref_t<T>;
            return traits::is_optional<plain>();
        }

        template <typename T>
        static constexpr bool is_string() {
            using plain = typename remove_optional<std::remove_cvref_t<T>>::type;
            return traits::is_basic_string<plain>();
        }

        template <typename T>
        static constexpr bool is_floating_point() {
            using plain = typename remove_optional<std::remove_cvref_t<T>>::type;
            return std::is_floating_point_v<plain>;
        }
    };

    template<typename Container, typename T>
    concept IndexableContainer = requires(const Container& c) {
        {c[0]} -> std::same_as<const T&>;
    };

    template<typename T>
    concept ContinuousContainer = ignore_qualifiers::is_continuous_container<T>();

    template<typename T>
    concept ArithmeticT = ignore_qualifiers::is_arithmetic<T>() || std::is_convertible_v<T, int> || std::is_convertible_v<T, float>;

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

    namespace __remove_from_tuple_detail {
        template <typename X, typename T, typename... Ts>
        struct remove : std::type_identity<T> {};

        template <typename X, typename... Ts, typename U, typename... Us>
        struct remove<X, std::tuple<Ts...>, U, Us...>
            : std::conditional_t<std::is_same_v<X, U>,
                                remove<X, std::tuple<Ts...>, Us...>,
                                remove<X, std::tuple<Ts..., U>, Us...>> {};
    };

    template <typename X, typename... Ts>
    using remove_from_tuple = typename __remove_from_tuple_detail::remove<X, std::tuple<>, Ts...>::type;

    template <typename T> struct is_tuple : std::false_type{};
    template <typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type{};

    namespace __flatten_tuple_detail {
        // utility to ensure return type is a tuple
        template<typename T>
            constexpr decltype(auto) as_tuple(T t) { return std::make_tuple(t); }

        template<typename ...Ts>
            constexpr decltype(auto) as_tuple(std::tuple<Ts...> t) { return t; }

        // Simple case
        template<typename T>
            constexpr decltype(auto) flatten(T t)
            {
                return t;
            }

        // No more recursion, (sizeof...Ts != 1) with above overload
        template<typename ...Ts, std::enable_if_t<!(is_tuple<Ts>::value || ...), bool> = false>
            constexpr decltype(auto) flatten(std::tuple<Ts...> t)
            {
                return t;
            }

        // Handle recursion
        template<typename ...Ts, std::enable_if_t<(is_tuple<Ts>::value || ...), bool> = false>
            constexpr decltype(auto) flatten(std::tuple<Ts...> t)
            {
                return std::apply([](auto...ts)
                        {
                        return flatten(std::tuple_cat(as_tuple(flatten(ts))...));
                        }, t);
            }
    };

    constexpr auto flatten_tuple(auto tuple) { return __flatten_tuple_detail::flatten(tuple); }
};
