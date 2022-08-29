#pragma once
#include <optional>
#include <algorithm>
#include "zxorm/common.hpp"

namespace zxorm {

template <FixedLengthString tableName, class T, class... Column>
class Table {
    public:
        void printColumns(T obj) {
            int i = 0;
            ((
              std::cout << "column[" << i << "] " << Column::name() << ": " << Column::getter(obj) << std::endl,
            i++), ...);
        }

        static constexpr std::string columnName(int idx) {
            int i = 0;
            std::string name;
            ((name = i == idx ? Column::name() : name, i++), ...);
            return name;
        }

        static constexpr std::string createTableQuery() {
            std::ostringstream query;
            query << "CREATE TABLE " << tableName.value << " (" << std::endl;
            ([&] {
                query << '\t' << Column::name() << " " << Column::type() << "," << std::endl;
            }(), ...);
            query << " );" << std::endl;
            return query.str();
        }

        std::optional<Error> create() { return std::nullopt; }
};

template <FixedLengthString columnName, auto Getter, auto Setter >
class ColumnPrivate {
    public:
    static constexpr const char* name() { return columnName.value; }
    static constexpr const char* type() { return "INT"; }
    static auto getter(auto obj) { return (obj.*Getter)(); };
    static void setter(auto obj, auto arg) { obj.*Setter(arg); };
};

template <FixedLengthString columnName, auto M>
class Column {
    public:
    static constexpr const char* name() { return columnName.value; }
    static constexpr const char* type() { return "INT"; }
    static auto getter(auto obj) { return obj.*M; };
    static void setter(auto obj, auto arg) { obj.*M = arg; };
};

};
