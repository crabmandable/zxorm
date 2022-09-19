#pragma once
#include <iostream>
#include <optional>
#include <algorithm>
#include <utility>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/column.hpp"

namespace zxorm {

template<class Column, class Table>
concept ColumnBelongingToClass = std::is_same<typename Column::ObjectClass, Table>::value;

template <FixedLengthString tableName, class T, ColumnBelongingToClass<T>... Column>
class Table {
    private:
        template <typename... C>
        struct FindPrimaryKey : std::false_type { };

        template <typename... C>
        requires AnyOf<Column::isPrimaryKey...>
        struct FindPrimaryKey<C...> : std::false_type
        {
            static constexpr int primaryKeyIndex = IndexOfFirst<C::isPrimaryKey...>::value;
            using type = typename std::tuple_element<primaryKeyIndex, std::tuple<Column...>>::type;
        };

    public:
        static std::string columnName(int idx) {
            int i = 0;
            std::string name;
            ((name = i == idx ? Column::name() : name, i++), ...);
            return name;
        }

        static constexpr int nColumns = std::tuple_size<std::tuple<Column...>>();
        static constexpr bool hasPrimaryKey = AnyOf<Column::isPrimaryKey...>;

        using PrimaryKey = typename FindPrimaryKey<Column...>::type;

        using ObjectClass = T;
        using columns_t = std::tuple<Column...>;

        static std::string createTableQuery(bool ifNotExist) {
            std::stringstream query;
            query << "CREATE TABLE ";
            if (ifNotExist)
                query << "IF NOT EXISTS ";
            query << tableName.value << " (\n";
            ([&] {
                query << '\t' << "`" << Column::name() << "` "
                    << sqlTypeStr(Column::SQLMemberType);

                auto constraints = Column::creationConstraints();
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

        static std::string findQuery() {
            std::ostringstream ss;
            ss << "SELECT * FROM `" << tableName.value << "` ";
            ss << "WHERE `" << PrimaryKey::name() << "` = ?;";

            return ss.str();

        }

        static std::string insertQuery() {
            std::ostringstream ss;
            ss << "INSERT INTO `" << tableName.value << "` (";

            size_t queryColumns = nColumns;
            std::apply([&](const auto&... a) {
                ([&]() {
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (column_t::isAutoIncColumn) {
                        queryColumns--;
                    } else {
                        ss << "`" << column_t::name() << "`, ";
                    }
                }(), ...);
            }, columns_t{});

            std::string str = ss.str();
            str.erase(str.end() - 2, str.end());

            std::ostringstream ss2;
            ss2 << ") VALUES (";
            for (size_t i = 0; i < queryColumns; i++) {
                if (i == queryColumns - 1) {
                    ss2 << "?);";
                } else  {
                    ss2 << "?, ";
                }
            }

            return str + ss2.str();
        }
};

};
