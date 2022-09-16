#pragma once

#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"

namespace zxorm {

    template <FixedLengthString columnName, auto M, class... Constraint>
    class Column {
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
        using MemberType = typename find_column_type<decltype(M)>::type;
        static_assert(!std::is_same<MemberType, std::false_type>::value, "Column template argument should be a pointer to a class member");
        using ObjectClass = typename find_column_type<decltype(M)>::klass;
        static constexpr const char* name() { return columnName.value; }
        static auto getter(auto obj) { return obj.*M; };
        static void setter(auto obj, auto arg) { obj.*M = arg; };

        static std::string creationConstraints() {
            std::ostringstream ss;
            appendToStringStream<Constraint...>(ss, " ");

            std::string qstr = ss.str();
            // erase trailing comma
            if (qstr.size() >= 2) {
                qstr.erase(qstr.end() - 1);
            }

            return qstr;
        }

        static constexpr sql_type_t SQLMemberType = MemberTypeToSQLType<MemberType>::value;

    };

    template <FixedLengthString columnName, auto Getter, auto Setter, class... Constraint>
    class ColumnPrivate {
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
        using MemberType = typename SetterResolved::argType;
        using ObjectClass = typename SetterResolved::klass;
        static constexpr const char* name() { return columnName.value; }
        static auto getter(auto obj) { return (obj.*Getter)(); };
        static void setter(auto obj, auto arg) { (obj.*Setter)(arg); };

        static std::string creationConstraints() {
            std::stringstream ss;
            ([&] {
                ss << Constraint::query() << ",";
            }(), ...);

            std::string qstr = ss.str();
            // erase trailing comma
            if (qstr.size() >= 2) {
                qstr.erase(qstr.end() - 1);
            }

            return qstr;
        }

        static constexpr sql_type_t SQLMemberType = MemberTypeToSQLType<MemberType>::value;
    };

};
