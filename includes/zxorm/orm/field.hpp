#pragma once
#include "zxorm/common.hpp"
#include "zxorm/orm/expression.hpp"

namespace zxorm {
    template <typename Table, FixedLengthString _name>
    struct Field {
        using column_t = typename Table::column_by_name<_name>::type;
        static_assert(not std::is_same_v<column_t, std::false_type>,
                "No such field belonging to table");

        static constexpr const char* name = _name.value;
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
        ColumnExpression<Table, column_t, comparison_op_t::GLOB, std::string> glob(std::string value) {
            return value;
        }

        template <typename M=void>
        requires (column_t::sql_column_type == sqlite_column_type::TEXT)
        ColumnExpression<Table, column_t, comparison_op_t::NOT_GLOB, std::string> not_glob(std::string value) {
            return value;
        }

        //TODO regexp
    };
};

