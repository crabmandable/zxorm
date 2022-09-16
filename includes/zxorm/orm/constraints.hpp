#pragma once

#include "zxorm/common.hpp"
#include <sstream>
#include <string>

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


    enum class action_t {
        NO_ACTION,
        RESTRICT,
        SET_NULL,
        SET_DEFAULT,
        CASCADE,
    };

    constexpr const char* actionStr(action_t c) {
        switch(c) {
            case action_t::NO_ACTION:
                return "NO_ACTION";
            case action_t::RESTRICT:
                return "RESTRICT";
            case action_t::SET_NULL:
                return "SET_NULL";
            case action_t::SET_DEFAULT:
                return "SET_DEFAULT";
            case action_t::CASCADE:
                return "CASCADE";
        };

        return "OOPS";
    }

    template<FixedLengthString tableName, FixedLengthString... column>
        struct Reference {
            static std::string to_string() {
                std::stringstream ss;
                ss << "REFERENCES `" << tableName.value << "` (";
                ([&]() {
                 ss << "`" << column.value << "`, ";
                 }(), ...);
                std::string str = ss.str();
                str.erase(str.end() - 2, str.end());
                return str + ")";
            }
        };

    // TODO support defferrable
    template<typename Reference, action_t onUpdate=action_t::NO_ACTION, action_t onDelete=action_t::NO_ACTION>
        struct ForeignKey {
            static std::string to_string() {
                std::stringstream ss;
                ss << Reference::to_string()
                    << " ON UPDATE " << actionStr(onUpdate)
                    << " ON DELETE " << actionStr(onDelete);
                return ss.str();
            }
        };

    // TODO support default with expr
    template<FixedLengthString value>
        struct Default {
            static std::string to_string() {
                std::stringstream ss;
                ss << "DEFAULT '" << value.value << "'";
                return ss.str();
            }
        };

    template<FixedLengthString value>
        struct Collate {
            static std::string to_string() {
                std::stringstream ss;
                ss << "COLLATE " << value.value;
                return ss.str();
            }
        };

    template<FixedLengthString constraint, conflict_t onConflict>
        struct ConstraintWithConflictClause {
            static std::string to_string() {
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
