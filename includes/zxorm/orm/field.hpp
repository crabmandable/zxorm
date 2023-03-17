#pragma once
#include "zxorm/common.hpp"
#include "zxorm/orm/expression.hpp"

namespace zxorm {
    template<typename Container>
    struct is_multi_value_binding : std::bool_constant<
        not traits::is_basic_string<Container>::value &&
        not traits::is_basic_string_view<Container>::value &&
            is_indexable_container<Container>::value
    >{};

    struct IsFieldTrait {};

    template <typename T>
    static constexpr bool is_field = std::is_base_of_v<IsFieldTrait, T>;

    template <typename Table, FixedLengthString _name>
    struct Field : IsFieldTrait {
        using column_t = typename Table::template column_by_name<_name>::type;
        static_assert(not std::is_same_v<column_t, std::false_type>,
                "No such field belonging to table");

        static constexpr auto name = _name;
        using table_t = Table;

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::EQ, M> operator==(M value) {
            return value;
        }

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::NE, M> operator!=(M value) {
            return value;
        }

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::LT, M> operator<(M value) {
            return value;
        }

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::LTE, M> operator<=(M value) {
            return value;
        }

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::GT, M> operator>(M value) {
            return value;
        }

        template <ArithmeticT M>
        requires (std::is_convertible_v<M, typename column_t::member_t>)
        ColumnExpression<Table, column_t, comparison_op_t::GTE, M> operator>=(M value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::LIKE, std::string> like(std::string value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_LIKE, std::string> not_like(std::string value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::LIKE, std::string_view> like(std::string_view value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_LIKE, std::string_view> not_like(std::string_view value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::LIKE, std::string> like(const char* value) {
            return std::string(value);
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_LIKE, std::string> not_like(const char* value) {
            return std::string(value);
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::GLOB, std::string> glob(std::string value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_GLOB, std::string> not_glob(std::string value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::GLOB, std::string_view> glob(std::string_view value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_GLOB, std::string_view> not_glob(std::string_view value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::GLOB, std::string> glob(const char* value) {
            return std::string(value);
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_GLOB, std::string> not_glob(const char* value) {
            return std::string(value);
        }



        template <typename Container>
        requires(is_multi_value_binding<Container>::value)
        ColumnExpression<Table, column_t, comparison_op_t::IN, Container> in(Container value) {
            return value;
        }

        template <typename Container>
        requires(is_multi_value_binding<Container>::value)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_IN, Container> not_in(Container value) {
            return value;
        }

        //TODO regexp
    };
};

