/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include "zxorm/common.hpp"
#include "zxorm/orm/statement.hpp"

namespace zxorm {
    // base class so that we can deal with any clause without having to know how it binds
    struct BindingClauseBase {
        std::string clause;
        BindingClauseBase(std::string clause) : clause{clause} {}
        virtual ~BindingClauseBase() = default;
        virtual void bind(Statement& s) = 0;
    };

    template <typename Bindings = std::tuple<>>
    struct BindingClause: public BindingClauseBase {
        Bindings bindings;

        BindingClause(std::string clause, Bindings bindings) : BindingClauseBase{clause}, bindings{bindings} {}

        void bind(Statement& s) {
            s.bind(bindings);
        }
    };

    template <typename Bindings = std::tuple<>>
    struct Where: public BindingClause<Bindings> {
        template <typename Expression>
        Where(const Expression& e) : BindingClause<Bindings>{std::string("WHERE ") + e.serialize(), e.bindings()} {}
    };

    template<bool _is_optional, typename Table>
    struct __selection {
        using table = Table;
        static constexpr bool is_optional = _is_optional;

        using result_t = typename table::object_class;

        using return_t = std::conditional_t<_is_optional,
          std::optional<typename table::object_class>,
          typename table::object_class
        >;

        static constexpr size_t n_columns = table::n_columns;

        static bool row_is_null(const auto& row) {
            return not Table::primary_key_t::getter(row);
        }

        static auto get_row(Statement& stmt, size_t column_offset = 0)
        {
            return table::get_row(stmt, column_offset);
        }

        friend std::ostream & operator<< (std::ostream &out, const __selection<_is_optional, Table>&) {
            out << "`" << Table::name.value << "`.*";
            return out;
        }
    };

    template<bool _is_optional, typename Table, FixedLengthString field_name>
    struct __selection<_is_optional, Field<Table, field_name>> {
        using table = Table;
        using column_t = typename Field<Table, field_name>::column_t;
        static constexpr bool is_optional = _is_optional;
        using field_type_t = typename column_t::member_t;
        using result_t = field_type_t;

        using return_t = std::conditional_t<_is_optional,
          std::optional<field_type_t>,
          field_type_t
        >;

        static constexpr size_t n_columns = 1;

        // Slightly dubious claim that having a ! operator, and being falsey
        // means the "row" is null
        // its possible this should actually just always return false?
        static bool row_is_null(const auto& row) {
            if constexpr (requires { (bool)!row; }) {
                return !row;
            } else {
                return false;
            }
        }

        static return_t get_row(Statement& stmt, size_t column_offset = 0)
        {
            return_t col;
            stmt.read_column(column_offset, col);
            return col;
        }

        friend std::ostream & operator<< (std::ostream &out, const __selection<_is_optional, Field<Table, field_name>>&) {
            out << "`" << Table::name.value << "`.`" << field_name.value << "`";
            return out;
        }
    };

    template<typename Field, bool distinct>
    struct __count {
        static constexpr bool is_optional = false;
        using return_t = unsigned long;
        using result_t = return_t;
        using table = typename Field::table_t;

        // counts are never null
        static bool row_is_null(const auto&) { return false; }

        static constexpr size_t n_columns = 1;

        static return_t get_row(Statement& stmt, size_t column_offset = 0)
        {
            return_t count;
            stmt.read_column(column_offset, count);
            return count;
        }

        friend std::ostream & operator<< (std::ostream &out, const __count<Field, distinct>&) {
            out << "COUNT(";
            if (distinct) {
                out << "DISTINCT ";
            }
            out << "`" << Field::table_t::name.value << "`.`" << Field::name.value << "`)";
            return out;
        }
    };

    template<typename From>
    struct __count_all {
        static constexpr bool is_optional = false;
        using return_t = unsigned long;
        using result_t = return_t;
        using table = From;

        // counts are never null
        static bool row_is_null(const auto&) { return false; }

        static constexpr size_t n_columns = 1;

        static return_t get_row(Statement& stmt, size_t column_offset = 0)
        {
            return_t count;
            stmt.read_column(column_offset, count);
            return count;
        }

        friend std::ostream & operator<< (std::ostream &out, const __count_all&) {
            out << "COUNT(*)";
            return out;
        }
    };

    /**
     *  __select_impl - This is the actual clause we use in the query
     *  @param From             - The table for the `FROM` clause
     *  @param Selections       - A list of `__selection` `__count`, or `__count_all`
     */
    template <typename From, typename... Selections>
    struct __select_impl {
        static_assert(sizeof...(Selections),
                "At least one table must be selected");

        using from_t = From;
        using selections_tuple = std::tuple<Selections...>;
        static constexpr bool is_multi_table_select = (std::tuple_size_v<unique_tuple<typename Selections::table...>>) > 1;

        template <typename T>
        struct _is_selection : std::false_type {};
        template <bool optional, typename T>
        struct _is_selection<__selection<optional, T>> : std::true_type {};

        template <typename... S>
        struct _selection_return {
            using type = std::tuple<typename S::return_t...>;
        };

        template <typename S>
        struct _selection_return <S> {
            using type = typename S::return_t;
        };

        using return_t = typename _selection_return<Selections...>::type;
        using result_t = std::tuple<std::optional<typename Selections::result_t>...>;

        friend std::ostream & operator<< (std::ostream &out, const __select_impl<From, Selections...>&) {
            out << "SELECT ";
                std::apply([&](const auto&... a) {
                    ([&]() {
                        out << a << ",";
                     }(), ...);
                }, std::tuple<Selections...>{});
            // erase trailing comma
            out.seekp(-1, std::ios_base::end);
            out << " ";
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
    struct __selectable_table {
        static constexpr auto is_optional = _is_optional;
        static constexpr auto table_name = _table_name;
    };

    template <typename TableToJoin, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_impl {
        static constexpr join_type_t join_type = type;

        template <typename T>
        struct referenced_by_one_of : std::false_type {};

        template <typename... T>
        struct referenced_by_one_of<std::tuple<T...>> : std::bool_constant<any_of<
            (TableToJoin::template does_reference_table<T::name> || T::template does_reference_table<TableToJoin::name>)...
        >> {};

        static_assert(referenced_by_one_of<AlreadyJoinedTuple>::value,
                "The queried tables do not contain a foreign key referencing this table");

        template <typename T>
        struct joined_table_meta {};

        template <typename... T>
        struct joined_table_meta<std::tuple<T...>> {
        private:
            static constexpr bool fk_is_already_joined() {
                int idx = index_of_first<T::template does_reference_table<TableToJoin::name>...>::value;
                return idx < 0;
            }

            static constexpr bool index() {
                int idx = index_of_first<T::template does_reference_table<TableToJoin::name>...>::value;
                if (idx < 0)
                    idx = index_of_first<TableToJoin::template does_reference_table<T::name>...>::value;

                return idx;
            }

        public:
            static constexpr int refers_to_index = index();
            using other_table_t = std::tuple_element_t<refers_to_index, std::tuple<T...>>;

            static constexpr const char* other_column_name() {
                if constexpr(fk_is_already_joined()) {
                    return TableToJoin::template foreign_column<other_table_t::name>::foreign_key_t::column_name.value;
                } else {
                    return other_table_t::template foreign_column<TableToJoin::name>::name.value;
                }
            }

            static constexpr const char* column_name() {
                if constexpr (fk_is_already_joined()) {
                    return TableToJoin::template foreign_column<other_table_t::name>::name.value;
                } else {
                    return other_table_t::template foreign_column<TableToJoin::name>::foreign_key_t::column_name.value;
                }
            }
        };

        using joined_meta_t = joined_table_meta<AlreadyJoinedTuple>;
        using other_table_t = typename joined_meta_t::other_table_t;

        static constexpr bool is_nested = joined_meta_t::refers_to_index > 0;

        static constexpr auto right_table_name = TableToJoin::name;
        static constexpr auto left_table_name = other_table_t::name;

        using selectables_tuple = std::tuple<
            __selectable_table<left_is_optional(type), left_table_name>,
            __selectable_table<right_is_optional(type), right_table_name>
        >;

        friend std::ostream & operator<< (std::ostream &out, const __join_impl<TableToJoin, type, AlreadyJoinedTuple>&) {
            out << type << "`" << TableToJoin::name.value << "`"
                << " ON `" << other_table_t::name.value << "`.`" << joined_meta_t::other_column_name() << "`"
                << " = `" << TableToJoin::name.value << "`.`" << joined_meta_t::column_name() << "`";

            return out;
        }
    };

    template <typename FieldA, typename FieldB, join_type_t type, typename AlreadyJoinedTuple>
    struct __join_on_impl {
        static constexpr join_type_t join_type = type;

        using table_a_t = typename FieldA::table_t;
        using table_b_t = typename FieldB::table_t;

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

        static constexpr bool is_nested = tuple_index<left_table_t, AlreadyJoinedTuple>::value > 0;

        static constexpr auto left_table_name = left_table_t::name;
        static constexpr auto left_field_name = left_is_a ? FieldA::name.value : FieldB::name.value;
        static constexpr auto right_table_name = right_table_t::name;
        static constexpr auto right_field_name = left_is_b ? FieldA::name.value : FieldB::name.value;

        using selectables_tuple = std::tuple<
            __selectable_table<left_is_optional(type), left_table_name>,
            __selectable_table<right_is_optional(type), right_table_name>
        >;

        friend std::ostream & operator<< (std::ostream &out, const __join_on_impl<FieldA, FieldB, type, AlreadyJoinedTuple>&) {
            out << type << "`" << right_table_name.value << "`"
                << " ON `" << left_table_name.value << "`.`" << left_field_name << "`"
                << " = `" << right_table_name.value << "`.`" << right_field_name << "`";

            return out;
        }
    };

    struct IsJoinTrait {};

    template <typename T, join_type_t type = join_type_t::INNER>
    struct Join : IsJoinTrait { };

    template <typename FieldA, typename FieldB, join_type_t type = join_type_t::INNER>
    struct JoinOn : IsJoinTrait { };

    template <typename T>
    static constexpr bool is_join = std::is_base_of_v<IsJoinTrait, T>;

    template <typename T>
    struct From {};


    struct IsCountTrait {};

    template<typename T, bool distinct = false>
    struct Count: IsCountTrait {};

    template<typename T>
    using CountDistinct = Count<T, true>;

    struct CountAll: IsCountTrait {};

    template <typename T>
    static constexpr bool is_count = std::is_base_of_v<IsCountTrait, T>;
};
