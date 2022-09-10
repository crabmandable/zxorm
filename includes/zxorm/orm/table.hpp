#pragma once
#include <iostream>
#include <optional>
#include <algorithm>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/column.hpp"

namespace zxorm {

template<class Column, class Table>
concept ColumnBelongingToClass = std::is_same<typename Column::ObjectClass, Table>::value;

template <FixedLengthString tableName, class T, ColumnBelongingToClass<T>... Column>
class Table {

    public:
        static constexpr std::string columnName(int idx) {
            int i = 0;
            std::string name;
            ((name = i == idx ? Column::name() : name, i++), ...);
            return name;
        }

        static constexpr int nColumns = std::tuple_size<std::tuple<Column...>>();

        static constexpr std::string createTableQuery(bool ifNotExist) {
            std::stringstream query;
            query << "CREATE TABLE ";
            if (ifNotExist)
                query << "IF NOT EXIST ";
            query << tableName.value << " (\n";
            ([&] {
                query << '\t'
                << "`" << Column::name() << "` "
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
};

};
