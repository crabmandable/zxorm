#pragma once
#include <optional>
#include <algorithm>
#include "zxorm/common.hpp"

namespace zxorm {

template<typename T>
struct MemberTypeToSQLType {
    static constexpr const char* value = "TEXT";
};

#define DEFINE_MEMBER_TO_SQL_TYPE(memberType, sqlType) \
template<> \
struct MemberTypeToSQLType<memberType> { \
    static constexpr const char* value = sqlType; \
};

DEFINE_MEMBER_TO_SQL_TYPE(int, "INT");
DEFINE_MEMBER_TO_SQL_TYPE(float, "REAL");
DEFINE_MEMBER_TO_SQL_TYPE(bool, "BOOLEAN");
DEFINE_MEMBER_TO_SQL_TYPE(void*, "BLOB");
#undef DEFINE_MEMBER_TO_SQL_TYPE


template <FixedLengthString tableName, class T, class... Column>
class Table {
    public:
        static constexpr void printTable() {
            int i = 0;
            std::cout << "Table: " << type_name<Table<tableName, T, Column...>>() << std::endl;
            std::cout << "name:"  << tableName.value << std::endl;
            ([&] {
                std::cout
                    << "column[" << i << "]: "
                    << Column::name() << " | "
                    << type_name<typename Column::MemberType>() << " | "
                    << MemberTypeToSQLType<typename Column::MemberType>::value << " | "
                    << std::endl;
            i++;
            }(), ...);
        }

        static constexpr std::string columnName(int idx) {
            int i = 0;
            std::string name;
            ((name = i == idx ? Column::name() : name, i++), ...);
            return name;
        }

        static constexpr int nColumns() {
            return std::tuple_size<std::tuple<Column...>>();
        }

        static constexpr std::string createTableQuery() {
            std::ostringstream query;
            query << "CREATE TABLE " << tableName.value << " (" << std::endl;
            ([&] {
                query << '\t'
                << Column::name() << " "
                << MemberTypeToSQLType<typename Column::MemberType>::value
                << "," << std::endl;
            }(), ...);

            // remove final comma & endl
            std::string qstr = query.str();
            qstr.erase(qstr.end() - 2);

            return qstr + " );";
        }

        std::optional<Error> create() { return std::nullopt; }
};

template <FixedLengthString columnName, auto Getter, auto Setter >
class ColumnPrivate {
    private:
    template< typename T >
    struct find_column_type : std::false_type
    {
    };

    template< typename R, typename C, class A >
    struct find_column_type< R (C::*)(A) >
    {
        typedef A type;
        typedef C klass;
    };


    public:
    using MemberType = find_column_type<decltype(Setter)>::type;
    using ObjectClass = find_column_type<decltype(Setter)>::klass;
    static constexpr const char* name() { return columnName.value; }
    static auto getter(auto obj) { return (obj.*Getter)(); };
    static void setter(auto obj, auto arg) { obj.*Setter(arg); };
};

template <FixedLengthString columnName, auto M>
class Column {
    template< typename T >
    struct find_column_type : std::false_type
    {
    };

    template< typename R, typename C>
    struct find_column_type< R C::* >
    {
        typedef R type;
        typedef C klass;
    };

    public:
    using MemberType = find_column_type<decltype(M)>::type;
    using ObjectClass = find_column_type<decltype(M)>::klass;
    static constexpr const char* name() { return columnName.value; }
    static auto getter(auto obj) { return obj.*M; };
    static void setter(auto obj, auto arg) { obj.*M = arg; };
};

};
