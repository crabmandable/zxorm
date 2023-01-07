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

    // This is the actual clause we use in the query
    template <typename From, typename... U>
    struct __select_impl {
        using from_t = From;
        using tables_t = std::tuple<From, U...>;

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
        OUTER,
        CROSS,
    };

    static inline std::ostream & operator<< (std::ostream &out, const join_type_t& type) {
        switch (type) {
            case join_type_t::INNER:
                out << "INNER JOIN ";
                break;
            case join_type_t::OUTER:
                out << "OUTER JOIN ";
                break;
            case join_type_t::CROSS:
                out << "CROSS JOIN ";
                break;
        };
        return out;
    };

    template <FixedLengthString foreign_table, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_impl {

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

        static constexpr auto joined_table_name = foreign_key_t::table_name;

        friend std::ostream & operator<< (std::ostream &out, const __join_impl<foreign_table, type, AlreadyJoinedTuple>&) {
            out << type << "`" << foreign_key_t::table_name.value << "`"
                << " ON `" << table_t::name.value << "`.`" << select_column::name.value << "`"
                << " = `" << foreign_key_t::table_name.value << "`.`" << foreign_key_t::column_name.value << "`";

            return out;
        }
    };

    template <Field field_a, Field field_b, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_on_impl {
        using field_a_t = decltype(field_a);
        using field_b_t = decltype(field_b);
        using table_a_t = typename field_a_t::table_t;
        using table_b_t = typename field_b_t::table_t;

        template <typename T>
        struct joined_is_b : std::false_type {};
        template <typename... T>
        struct joined_is_b<std::tuple<T...>> : std::bool_constant<any_of<std::is_same_v<table_a_t, T>...>> {};

        template <typename T>
        struct joined_is_a : std::false_type {};

        template <typename... T>
        struct joined_is_a<std::tuple<T...>> : std::bool_constant<any_of<std::is_same_v<table_b_t, T>...>> {};

        static_assert(joined_is_a<AlreadyJoinedTuple>::value || joined_is_b<AlreadyJoinedTuple>::value,
                "One of the join fields should belong a table being queried, "
                "or another table already joined");

        static constexpr auto joined_table_name = joined_is_b<AlreadyJoinedTuple>::value ? table_b_t::name : table_a_t::name;

        friend std::ostream & operator<< (std::ostream &out, const __join_on_impl<field_a, field_b, type, AlreadyJoinedTuple>&) {
            // using this lambda to get arround the fact that we have a constexpr if
            // which means I can't just make the join_table, join_field and select_fields variables,
            // since they're const and have to be initialized on declaration
            // pretty lame...
            auto serialize = [&out](const char* join_table, const char* join_field, const char * select_table, const char* select_field) {
                out << type << "`" << join_table << "`"
                    << " ON `" << select_table << "`.`" << select_field << "`"
                    << " = `" << join_table << "`.`" << join_field << "`";
            };

            if constexpr (joined_is_b<AlreadyJoinedTuple>::value) {
                serialize(table_b_t::name.value, field_b_t::name, table_a_t::name.value, field_a_t::name);
            } else {
                serialize(table_a_t::name.value, field_a_t::name, table_b_t::name.value, field_b_t::name);
            }

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
