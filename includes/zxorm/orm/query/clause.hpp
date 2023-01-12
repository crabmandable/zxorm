#pragma once
namespace zxorm {
    // base class so that we can deal with any clause without having to know how it binds
    struct BindingClauseBase {
        std::string clause;
        BindingClauseBase(std::string clause) : clause{clause} {}
        virtual OptionalError bind(Statement& s) = 0;
    };

    template <typename Bindings = std::tuple<>>
    struct BindingClause: public BindingClauseBase {
        Bindings bindings;

        BindingClause(std::string clause, Bindings bindings) : BindingClauseBase{clause}, bindings{bindings} {}

        OptionalError bind(Statement& s) {
            return s.bind(bindings);
        }
    };

    template <typename Bindings = std::tuple<>>
    struct Where: public BindingClause<Bindings> {
        template <typename Expression>
        Where(const Expression& e) : BindingClause<Bindings>{std::string("WHERE ") + e.serialize(), e.bindings()} {}
    };

    template<bool _is_optional, typename Table>
    struct __selectable {
        using table = Table;
        static constexpr bool is_optional = _is_optional;
    };

    // This is the actual clause we use in the query
    template <typename From, typename... U>
    struct __select_impl {
        using from_t = From::table;
        using tables_t = std::tuple<typename From::table, typename U::table...>;

        static constexpr std::array<bool, sizeof...(U) + 1> is_optional = {
            From::is_optional, U::is_optional...
        };

        using return_t = std::conditional_t<sizeof...(U),
            std::tuple<
                std::conditional_t<From::is_optional, std::optional<typename From::table::object_class>, typename From::table::object_class>,
                std::conditional_t<U::is_optional, std::optional<typename U::table::object_class>, typename U::table::object_class>...
            >,
            typename From::table::object_class
        >;

        friend std::ostream & operator<< (std::ostream &out, const __select_impl<From, U...>&) {
            out << "SELECT ";
                std::apply([&](const auto&... a) {
                    ([&]() {
                        using table_t = std::remove_reference_t<decltype(a)>;
                        out << "`" << table_t::name.value << "`.*,";
                     }(), ...);
                }, tables_t{});
            // erase trailing comma
            out.seekp(-1, std::ios_base::end);
            return out;
        }
    };
    // This is user facing interface used by the connection to create the above struct
    template <typename From, typename... U>
    struct Select {};

    template <typename T>
    struct is_select : std::false_type {};

    template <typename From, typename... U>
    struct is_select<Select<From, U...>> : std::true_type {};

    template <typename T>
    static constexpr bool is_select_v = is_select<T>::value;

    enum join_type_t {
        INNER,
        LEFT_OUTER,
        RIGHT_OUTER,
        FULL_OUTER,
        CROSS,
    };

    static inline std::ostream & operator<< (std::ostream &out, const join_type_t& type) {
        switch (type) {
            case join_type_t::INNER:
                out << "INNER JOIN ";
                break;
            case join_type_t::LEFT_OUTER:
                out << "LEFT OUTER JOIN ";
                break;
            case join_type_t::RIGHT_OUTER:
                out << "RIGHT OUTER JOIN ";
                break;
            case join_type_t::FULL_OUTER:
                out << "FULL OUTER JOIN ";
                break;
            case join_type_t::CROSS:
                out << "CROSS JOIN ";
                break;
        };
        return out;
    };

    static inline constexpr bool left_is_optional(join_type_t t)
    {
        return t == join_type_t::FULL_OUTER ||
            t == join_type_t::RIGHT_OUTER;
    };

    static inline constexpr bool right_is_optional(join_type_t t)
    {
        return t == join_type_t::FULL_OUTER ||
            t == join_type_t::LEFT_OUTER;
    };

    template<bool _is_optional, FixedLengthString _table_name>
    struct __joined_table {
        static constexpr auto is_optional = _is_optional;
        static constexpr auto table_name = _table_name;
    };

    template <FixedLengthString foreign_table, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_impl {
        static constexpr join_type_t join_type = type;

        template <typename T>
        struct referenced_by_one_of : std::false_type {};

        template <typename... T>
        struct referenced_by_one_of<std::tuple<T...>> : std::bool_constant<any_of<T::template does_reference_table<foreign_table>...>> {};

        static_assert(referenced_by_one_of<AlreadyJoinedTuple>::value,
                "The queried tables do not contain a foreign key referencing this table");

        template <typename T>
        struct index_of_table { static constexpr int value = -1; };

        template <typename... T>
        struct index_of_table<std::tuple<T...>> {
            static constexpr int value = index_of_first<T::template does_reference_table<foreign_table>...>::value;
        };

        using table_t = std::tuple_element_t<index_of_table<AlreadyJoinedTuple>::value, AlreadyJoinedTuple>;

        using select_column = typename table_t::foreign_column<foreign_table>;
        using foreign_key_t = typename select_column::foreign_key_t;

        static constexpr auto right_table_name = foreign_key_t::table_name;
        static constexpr auto left_table_name = table_t::name;

        using selectables_tuple = std::tuple<
            __joined_table<left_is_optional(type), left_table_name>,
            __joined_table<right_is_optional(type), right_table_name>
        >;

        friend std::ostream & operator<< (std::ostream &out, const __join_impl<foreign_table, type, AlreadyJoinedTuple>&) {
            out << type << "`" << foreign_key_t::table_name.value << "`"
                << " ON `" << table_t::name.value << "`.`" << select_column::name.value << "`"
                << " = `" << foreign_key_t::table_name.value << "`.`" << foreign_key_t::column_name.value << "`";

            return out;
        }
    };

    template <Field field_a, Field field_b, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_on_impl {
        static constexpr join_type_t join_type = type;

        using field_a_t = decltype(field_a);
        using field_b_t = decltype(field_b);
        using table_a_t = typename field_a_t::table_t;
        using table_b_t = typename field_b_t::table_t;

        template <typename T>
        struct _left_is_b : std::false_type {};
        template <typename... T>
        struct _left_is_b<std::tuple<T...>> : std::bool_constant<any_of<std::is_same_v<table_b_t, T>...>> {};
        static constexpr bool left_is_b = _left_is_b<AlreadyJoinedTuple>::value;

        template <typename T>
        struct _left_is_a : std::false_type {};
        template <typename... T>
        struct _left_is_a<std::tuple<T...>> : std::bool_constant<any_of<std::is_same_v<table_a_t, T>...>> {};
        static constexpr bool left_is_a = _left_is_a<AlreadyJoinedTuple>::value;

        static_assert(left_is_a || left_is_b,
                "One of the join fields should belong a table being queried, "
                "or another table already joined");

        using left_table_t = std::conditional_t<left_is_a, table_a_t, table_b_t>;
        using right_table_t = std::conditional_t<left_is_b, table_a_t, table_b_t>;

        static constexpr auto left_table_name = left_table_t::name;
        static constexpr auto left_field_name = left_is_a ? field_a_t::name : field_b_t::name;
        static constexpr auto right_table_name = right_table_t::name;
        static constexpr auto right_field_name = left_is_b ? field_a_t::name : field_b_t::name;

        using selectables_tuple = std::tuple<
            __joined_table<left_is_optional(type), left_table_name>,
            __joined_table<right_is_optional(type), right_table_name>
        >;

        friend std::ostream & operator<< (std::ostream &out, const __join_on_impl<field_a, field_b, type, AlreadyJoinedTuple>&) {
            out << type << "`" << right_table_name.value << "`"
                << " ON `" << left_table_name.value << "`.`" << left_field_name << "`"
                << " = `" << right_table_name.value << "`.`" << right_field_name << "`";

            return out;
        }
    };

    struct IsJoinTrait {};

    template <typename T, join_type_t type = join_type_t::INNER>
    struct Join : IsJoinTrait {};

    template <Field field_a, Field field_b, join_type_t type = join_type_t::INNER>
    struct JoinOn : IsJoinTrait {};

    template <typename T>
    static constexpr bool is_join = std::is_base_of_v<IsJoinTrait, T>;

};
