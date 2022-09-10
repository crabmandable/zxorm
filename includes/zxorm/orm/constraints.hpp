#pragma once

#include "zxorm/common.hpp"
namespace zxorm {
    enum class conflict_t {
        ABORT,
        ROLLBACK,
        FAIL,
        IGNORE,
        REPLACE,
    };

    constexpr const char* conflictStr(conflict_t c) {
        switch(c) {
            case conflict_t::ROLLBACK:
                return "ROLLBACK";
            case conflict_t::ABORT:
                return "ABORT";
            case conflict_t::FAIL:
                return "FAIL";
            case conflict_t::IGNORE:
                return "IGNORE";
            case conflict_t::REPLACE:
                return "REPLACE";
        };

        return "OOPS";
    }

    namespace column_constraint {

        // TODO support default with expr
        template<FixedLengthString value>
        struct Default {
            constexpr static std::string to_string() {
                std::stringstream ss;
                ss << "DEFAULT '" << value.value << "'";
                return ss.str();
            }
        };

        template<FixedLengthString value>
        struct Collate {
            constexpr static std::string to_string() {
                std::stringstream ss;
                ss << "COLLATE " << value.value;
                return ss.str();
            }
        };

        template<FixedLengthString constraint, conflict_t onConflict>
        struct ConstraintWithConflictClause {
            constexpr static std::string to_string() {
                std::stringstream ss;
                ss << constraint.value << " ON CONFLICT " << conflictStr(onConflict);
                return ss.str();
            }
        };

        template<conflict_t onConflict=conflict_t::ABORT>
        using Unique = ConstraintWithConflictClause<"UNIQUE", onConflict>;

        template<conflict_t onConflict=conflict_t::ABORT>
        using NotNull = ConstraintWithConflictClause<"NOT NULL", onConflict>;

        template<conflict_t onConflict=conflict_t::ABORT>
        using PrimaryKey = ConstraintWithConflictClause<"PRIMARY KEY", onConflict>;

        template<conflict_t onConflict=conflict_t::ABORT>
        using PrimaryKeyAsc = ConstraintWithConflictClause<"PRIMARY KEY ASC", onConflict>;

        template<conflict_t onConflict=conflict_t::ABORT>
        using PrimaryKeyDesc = ConstraintWithConflictClause<"PRIMARY KEY DESC", onConflict>;

        //TODO add `CHECK` constraint

    };
};
