#pragma once
#include <iostream>
#include <optional>
#include <utility>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/column.hpp"
#include "zxorm/orm/field.hpp"

namespace zxorm {

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

        template <FixedLengthString name>
        static inline Field<Table, name> field = Field<Table, name>();

        static constexpr auto name = table_name.value;

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

        static std::string insert_query() {
            std::ostringstream ss;
            ss << "INSERT INTO `" << table_name.value << "` (";

            size_t n_query_columns = n_columns;
            std::apply([&](const auto&... a) {
                ([&]() {
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (column_t::is_auto_inc_column) {
                        n_query_columns--;
                    } else {
                        ss << "`" << column_t::name.value << "`, ";
                    }
                }(), ...);
            }, columns_t{});

            std::string str = ss.str();
            str.erase(str.end() - 2, str.end());

            std::ostringstream ss2;
            ss2 << ") VALUES (";
            for (size_t i = 0; i < n_query_columns; i++) {
                if (i == n_query_columns - 1) {
                    ss2 << "?);";
                } else  {
                    ss2 << "?, ";
                }
            }

            return str + ss2.str();
        }

        static std::string update_query() {
            std::ostringstream ss;
            ss << "UPDATE `" << table_name.value << "` SET ";

            std::apply([&](const auto&... a) {
                ([&]() {
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (not column_t::is_primary_key) {
                        ss << "`" << column_t::name.value << "` = ?, ";
                    }
                }(), ...);
            }, columns_t{});

            std::string str = ss.str();
            str.erase(str.end() - 2, str.end());

            return str + " WHERE `" + primary_key_t::name.value + "` = ?;";
        }

        static Result<T> get_row(Statement& stmt)
        {
            if (stmt.column_count() != n_columns) {
                return Error("Unexpected number of columns returned by query,"
                        " tables may not be synced");
            }

            T record;
            size_t column_idx = 0;
            OptionalError err;
            std::apply([&](const auto&... a) {
                ([&]() {
                    if (err) return;
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (column_t::public_column) {
                        err = stmt.read_column(column_idx++, column_t::getter(record));
                    } else {
                        using value_t = typename column_t::member_t;
                        value_t value;
                        err = stmt.read_column(column_idx++, value);
                        column_t::setter(record, value);
                    }
                }(), ...);
            }, columns_t{});

            if (err) {
                return err.value();
            }

            return record;
        }
};

};
