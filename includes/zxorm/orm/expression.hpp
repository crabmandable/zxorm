#pragma once
#include "zxorm/common.hpp"

namespace zxorm {
    enum class comparison_op_t {
        EQ,
        NE,
        LT,
        LTE,
        GT,
        GTE,
        LIKE,
        NOT_LIKE,
        GLOB,
        NOT_GLOB,
        IN,
        NOT_IN,
    };

    static inline std::ostream & operator<< (std::ostream &out, const comparison_op_t& op) {
        switch (op) {
            case comparison_op_t::EQ:
                out << " = ";
                break;
            case comparison_op_t::NE:
                out << " != ";
                break;
            case comparison_op_t::LT:
                out << " < ";
                break;
            case comparison_op_t::LTE:
                out << " <= ";
                break;
            case comparison_op_t::GT:
                out << " > ";
                break;
            case comparison_op_t::GTE:
                out << " >= ";
                break;
            case comparison_op_t::LIKE:
                out << " LIKE ";
                break;
            case comparison_op_t::NOT_LIKE:
                out << " NOT LIKE ";
                break;
            case comparison_op_t::GLOB:
                out << " GLOB ";
                break;
            case comparison_op_t::NOT_GLOB:
                out << " NOT GLOB ";
                break;
            case comparison_op_t::IN:
                out << " IN ";
                break;
            case comparison_op_t::NOT_IN:
                out << " NOT IN ";
                break;
        }
        return out;
    }

    enum class boolean_op_t {
        AND,
        OR,
    };

    static inline std::ostream & operator<< (std::ostream &out, const boolean_op_t& op) {
        switch (op) {
            case boolean_op_t::AND:
                out << " and ";
                break;
            case boolean_op_t::OR:
                out << " or ";
                break;
        }
        return out;
    }

    template <boolean_op_t op, typename LHS, typename RHS>
    struct ExpressionExpression {

        using tables_t = decltype(std::tuple_cat(
            std::declval<typename std::remove_cvref_t<LHS>::tables_t>(),
            std::declval<typename std::remove_cvref_t<RHS>::tables_t>()
        ));

        LHS lhs;
        RHS rhs;
        ExpressionExpression(LHS lhs, RHS rhs) : lhs{lhs}, rhs{rhs} {}

        template<typename Expression>
        auto operator&&(Expression other) {
            return ExpressionExpression<boolean_op_t::AND, decltype(*this), Expression>(*this, other);
        }

        template<typename Expression>
        auto operator||(Expression other) {
            return ExpressionExpression<boolean_op_t::OR, decltype(*this), Expression>(*this, other);
        }

        std::string serialize() const {
            std::stringstream ss;
            ss << "(" << lhs << op << rhs << ")";
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out, const ExpressionExpression& e) {
            out << e.serialize();
            return out;
        }

        auto bindings() const {
            return std::tuple_cat(lhs.bindings(), rhs.bindings());
        }
    };

    template <typename Table, typename Column, comparison_op_t op, typename RHS>
    struct ColumnExpression {
        using tables_t = std::tuple<Table>;
        RHS to_bind;
        ColumnExpression(RHS rhs) : to_bind{rhs} {}

        template<typename Expression>
        auto operator&&(Expression other) {
            return ExpressionExpression<boolean_op_t::AND, decltype(*this), Expression>(*this, other);
        }

        template<typename Expression>
        auto operator||(Expression other) {
            return ExpressionExpression<boolean_op_t::OR, decltype(*this), Expression>(*this, other);
        }

        std::string serialize() const {
            std::stringstream ss;
            ss << "`" << Table::name.value << "`.`" << Column::name.value << "` " << op;
            if constexpr (op == comparison_op_t::IN || op == comparison_op_t::NOT_IN) {
                ss << " (";
                for (size_t i = 0; i < to_bind.size(); i++) {
                    ss << "?,";
                }
                ss.seekp(-1, std::ios_base::end);
                ss << ") ";
            } else {
                ss << " ?";
            }
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out, const ColumnExpression& e) {
            out << e.serialize();
            return out;
        }

        auto bindings() const {
            return std::tuple(to_bind);
        }
    };

};
