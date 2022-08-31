#pragma once
#include <optional>
#include <algorithm>
#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"

namespace zxorm {

template<class Column, class Table>
concept ColumnBelongingToClass = std::is_same<typename Column::ObjectClass, Table>::value;

template <FixedLengthString tableName, class T, ColumnBelongingToClass<T>... Column>
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
                    << sqlTypeStr(Column::SQLMemberType) << " | "
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
                << sqlTypeStr(Column::SQLMemberType)
                << "," << std::endl;
            }(), ...);

            // remove final comma & endl
            std::string qstr = query.str();
            qstr.erase(qstr.end() - 2);

            return qstr + " );";
        }

        std::optional<Error> create() { return std::nullopt; }
};

template <FixedLengthString columnName, auto Getter, auto Setter=nullptr>
class Column {
    private:
    template< typename T >
    struct resolve_function_ptr_types : std::false_type
    {
        using argType = std::false_type;
        using returnType = void;
        using klass = std::false_type;
    };

    // pointer to setter
    template<typename C, class A >
    struct resolve_function_ptr_types< void (C::*)(A) >
    {
        using argType = A;
        using klass = C;
        using returnType = void;
    };

    // pointer to getter
    template< typename R, typename C  >
    struct resolve_function_ptr_types< R (C::*)() >
    {
        using argType = std::false_type;
        using klass = C;
        using returnType = R;
    };

    // pointer to member
    template< typename R, typename C>
    struct resolve_function_ptr_types< R C::* >
    {
        using argType = std::false_type;
        using returnType = void;
        using klass = std::false_type;
    };

    using SetterResolved = resolve_function_ptr_types<decltype(Setter)>;
    using GetterResolved = resolve_function_ptr_types<decltype(Getter)>;

    static_assert(!std::is_same<typename SetterResolved::argType, std::false_type>::value,
        "Column template argument should be a pointer to a class method that sets the column data");
    static_assert(std::is_same<typename SetterResolved::returnType, void>::value,
        "Column template argument should be a pointer to a class method that sets the column data. The return type should be `void`");

    static_assert(std::is_same<typename GetterResolved::argType, std::false_type>::value,
        "Column template argument should be a pointer to a class method that gets the column data");
    static_assert(!std::is_same<typename GetterResolved::returnType, void>::value,
        "Column template argument should be a pointer to a class method that gets the column data. The return type should not be `void`");

    static_assert(std::is_same<typename GetterResolved::returnType, typename SetterResolved::argType>::value,
            "Column template arguments should be a pointers to class methods that get and set the column data");

    public:
    using MemberType = SetterResolved::argType;
    using ObjectClass = SetterResolved::klass;
    static constexpr const char* name() { return columnName.value; }
    static auto getter(auto obj) { return (obj.*Getter)(); };
    static void setter(auto obj, auto arg) { obj.*Setter(arg); };

    static constexpr sql_type_t SQLMemberType = MemberTypeToSQLType<MemberType>::value;
};

template <FixedLengthString columnName, auto M>
class Column<columnName, M, nullptr> {
    template< typename T >
    struct find_column_type : std::false_type
    {
        using type = std::false_type;
    };

    template< typename R, typename C, class A>
    struct find_column_type< R (C::*)(A) >
    {
        using type = std::false_type;
    };

    template< typename R, typename C>
    struct find_column_type< R C::* >
    {
        using type = R;
        using klass = C;
    };

    public:
    using MemberType = find_column_type<decltype(M)>::type;
    static_assert(!std::is_same<MemberType, std::false_type>::value, "Column template argument should be a pointer to a class member");
    using ObjectClass = find_column_type<decltype(M)>::klass;
    static constexpr const char* name() { return columnName.value; }
    static auto getter(auto obj) { return obj.*M; };
    static void setter(auto obj, auto arg) { obj.*M = arg; };

    static constexpr sql_type_t SQLMemberType = MemberTypeToSQLType<MemberType>::value;

};

};
