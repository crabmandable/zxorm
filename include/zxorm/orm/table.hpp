/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include <iostream>
#include <optional>
#include <utility>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/column.hpp"
#include "zxorm/orm/field.hpp"
#include "zxorm/error.hpp"

namespace zxorm {
    namespace __foreign_key_detail {

        template <typename T, typename... Ts>
        struct foreign_only_impl: std::type_identity<T> {};

        template <typename... Ts, typename U, typename... Us>
        struct foreign_only_impl<std::tuple<Ts...>, U, Us...>
            : std::conditional_t<(not std::is_same_v<std::false_type, typename U::foreign_key_t>)
                                , foreign_only_impl<std::tuple<Ts..., U>, Us...>
                                , foreign_only_impl<std::tuple<Ts...>, Us...>> {};

        template <typename... Columns>
        using foreign_only_t = typename foreign_only_impl<std::tuple<>, Columns...>::type;

        template<FixedLengthString foreign_table, typename... foreign_columns>
        static constexpr std::array<bool, sizeof...(foreign_columns)> does_column_reference_table {
            (foreign_columns::foreign_key_t::table_name == foreign_table)...
        };

        // unused base case, just needed for specializtion to unwrap tuple
        template<FixedLengthString, typename T>
        struct index_of_foreign_table {
            static_assert(std::is_same_v<std::false_type, T>, "index of foreign table should only be used with a tuple");
            int value = -1;
        };

        template<FixedLengthString foreign_table, typename ...T>
        struct index_of_foreign_table<foreign_table, std::tuple<T...>>
        {
            private:
            static constexpr int _impl() {
                constexpr auto answers = does_column_reference_table<foreign_table, T...>;
                const auto it = std::find(answers.begin(), answers.end(), true);

                if (it == answers.end()) return -1;
                return std::distance(answers.begin(), it);
            }

            public:
            static constexpr int value = _impl();
        };

        template <FixedLengthString foreign_table, typename foreign_key_tuple>
        struct column_with_reference {};

        template <FixedLengthString foreign_table, typename foreign_key_tuple>
        requires (index_of_foreign_table<foreign_table, foreign_key_tuple>::value >= 0)
        struct column_with_reference<foreign_table, foreign_key_tuple> : std::type_identity<
            std::tuple_element_t<index_of_foreign_table<foreign_table, foreign_key_tuple>::value, foreign_key_tuple>
        > {};

        template <FixedLengthString foreign_table, typename foreign_key_tuple>
        using column_with_reference_t = typename column_with_reference<foreign_table, foreign_key_tuple>::type;
    };

template <FixedLengthString table_name, class T, class... Column>
class Table {
    static_assert(all_of<std::is_same<typename Column::object_class, T>::value...>,
        "Columns must all belong to the table"
    );
    static_assert(std::is_default_constructible_v<T>, "Record type must have a default constructor");
    public:
        template <typename... C>
        struct find_primary_key {
            static constexpr int primary_key_index = -1;
            using type = std::false_type;
        };

        template <typename... C>
        requires any_of<Column::is_primary_key...>
        struct find_primary_key<C...>
        {
            static constexpr int primary_key_index = index_of_first<C::is_primary_key...>::value;
            using type = typename std::tuple_element<primary_key_index, std::tuple<Column...>>::type;
        };

        template <FixedLengthString name>
        struct column_by_name
        {
            static constexpr int column_idx = -1;
            using type = std::false_type;
        };

        template <FixedLengthString name>
        requires (index_of_first<name == Column::name...>::value >= 0)
        struct column_by_name<name>
        {
            static constexpr int column_idx = index_of_first<name == Column::name...>::value;
            using type = typename std::tuple_element<column_idx, std::tuple<Column...>>::type;
        };

        static constexpr int n_columns = std::tuple_size<std::tuple<Column...>>();
        static constexpr bool has_primary_key = any_of<Column::is_primary_key...>;

        using primary_key_t = typename find_primary_key<Column...>::type;

        using object_class = T;
        using columns_t = std::tuple<Column...>;

        using foreign_columns_t = __foreign_key_detail::foreign_only_t<Column...>;


        template <FixedLengthString foreign_table>
        static constexpr bool does_reference_table = __foreign_key_detail::index_of_foreign_table<foreign_table, foreign_columns_t>::value >= 0;

        template <FixedLengthString foreign_table>
        using foreign_column = std::remove_reference_t<__foreign_key_detail::column_with_reference_t<foreign_table, foreign_columns_t>>;

        static void print_foreign_keys() {
            std::apply([&]<typename... U>(const U&...) {
                ([&]() {
                    std::cout << "column: " << U::name.value << std::endl;
                    std::cout << "foreign key: " << std::endl;
                    using fk_t = typename U::foreign_key_t;
                    std::cout << "\t" << fk_t::table_name.value << "," << fk_t::column_name.value << std::endl;
                }(), ...);
            }, foreign_columns_t{});
        }

        template <FixedLengthString name>
        using field_t = Field<Table, name>;

        static constexpr auto name = table_name;

        static std::string create_table_query(bool if_not_exist) {
            std::stringstream query;
            query << "CREATE TABLE ";
            if (if_not_exist)
                query << "IF NOT EXISTS ";
            query << table_name.value << " (\n";
            ([&] {
                query << '\t' << "`" << Column::name.value << "` "
                    << sql_type_str(Column::sql_column_type);

                auto constraints = Column::constraint_creation_query();
                if (!constraints.empty()) {
                    query << " " << constraints;
                }
                query << ",\n";
            }(), ...);

            // remove final comma & endl
            std::string qstr = query.str();
            qstr.erase(qstr.end() - 2);

            return qstr + " );\n";
        }

        static std::string insert_query(size_t n_rows = 1) {
            std::ostringstream ss;
            ss << "INSERT INTO `" << table_name.value << "` (";

            // used to tell us when to insert a NULL for the auto increment column
            bool auto_inc_idx = -1;

            std::apply([&]<typename... U>(const U&...) {
                int i = 0;
                ([&]() {
                    if constexpr (U::is_auto_inc_column) {
                        auto_inc_idx = i;
                    }

                    ss << "`" << U::name.value << "`, ";
                    i++;
                }(), ...);
            }, columns_t{});

            std::string str = ss.str();
            str.erase(str.end() - 2, str.end());

            std::ostringstream ss2;
            ss2 << ") VALUES ";

            for (size_t i = 0; i < n_rows; i++) {
                std::ostringstream val;
                val << "(";
                for (size_t i = 0; i < n_columns; i++) {
                    // auto inc column gets a null
                    val << (i == auto_inc_idx ? "NULL" : "?");
                    // last column needs to close the paren
                    val << (i == n_columns - 1 ? "), " : ", ");
                }

                auto v = val.str();
                if (i == n_rows - 1) {
                    v.erase(v.end() - 2, v.end());
                }
                ss2 << v;
            }

            return str + ss2.str() + ";";
        }

        static std::string update_query() {
            std::ostringstream ss;
            ss << "UPDATE `" << table_name.value << "` SET ";

            std::apply([&]<typename... U>(const U&...) {
                ([&]() {
                    if constexpr (not U::is_primary_key) {
                        ss << "`" << U::name.value << "` = ?, ";
                    }
                }(), ...);
            }, columns_t{});

            std::string str = ss.str();
            str.erase(str.end() - 2, str.end());

            return str + " WHERE `" + primary_key_t::name.value + "` = ?;";
        }

        static T get_row(Statement& stmt, size_t column_offset = 0)
        {
            if (stmt.column_count() - column_offset < n_columns) {
                throw ConnectionError("Unexpected number of columns returned by query,"
                        " tables may not be synced");
            }

            T record;
            size_t column_idx = column_offset;
            std::apply([&]<typename... U>(const U&...) {
                ([&]() {
                    if constexpr (U::public_column) {
                        stmt.read_column(column_idx++, U::getter(record));
                    } else {
                        using value_t = typename U::member_t;
                        value_t value;
                        stmt.read_column(column_idx++, value);
                        U::setter(record, value);
                    }
                }(), ...);
            }, columns_t{});

            return record;
        }
};

template <typename T>
struct is_table : std::false_type {};

template <FixedLengthString table_name, class T, class... Column>
struct is_table<Table<table_name, T, Column...>> : std::true_type {};

template <typename T>
static constexpr bool is_table_v = is_table<T>::value;

};
