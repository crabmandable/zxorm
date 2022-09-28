#pragma once
#include <iostream>
#include <optional>
#include <utility>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/column.hpp"

namespace zxorm {

template<class Column, class Table>
concept ColumnBelongingToClass = std::is_same<typename Column::object_class, Table>::value;

template <FixedLengthString table_name, class T, ColumnBelongingToClass<T>... Column>
class Table {
    private:
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

    public:
        static std::string column_name(int idx) {
            int i = 0;
            std::string name;
            ((name = i == idx ? Column::name() : name, i++), ...);
            return name;
        }

        static constexpr int n_columns = std::tuple_size<std::tuple<Column...>>();
        static constexpr bool has_primary_key = any_of<Column::is_primary_key...>;

        using primary_key_t = typename find_primary_key<Column...>::type;

        using object_class = T;
        using columns_t = std::tuple<Column...>;

        static std::string create_table_query(bool if_not_exist) {
            std::stringstream query;
            query << "CREATE TABLE ";
            if (if_not_exist)
                query << "IF NOT EXISTS ";
            query << table_name.value << " (\n";
            ([&] {
                query << '\t' << "`" << Column::name() << "` "
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

        static std::string find_query() {
            std::ostringstream ss;
            ss << "SELECT * FROM `" << table_name.value << "` ";
            ss << "WHERE `" << primary_key_t::name() << "` = ?;";

            return ss.str();
        }

        static std::string delete_query() {
            std::ostringstream ss;
            ss << "DELETE FROM `" << table_name.value << "` ";
            ss << "WHERE `" << primary_key_t::name() << "` = ?;";

            return ss.str();
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
                        ss << "`" << column_t::name() << "`, ";
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
};

};
