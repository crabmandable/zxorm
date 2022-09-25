#pragma once

#include "zxorm/common.hpp"
#include <sstream>
#include <string>
#include <type_traits>

namespace zxorm {
    enum class conflict_t {
        abort,
        rollback,
        fail,
        ignore,
        replace,
    };

    enum class action_t {
        no_action,
        restrict,
        set_null,
        set_default,
        cascade,
    };

    namespace __constraint_enum_to_str {
        constexpr const char* actionStr(action_t c) {
            switch(c) {
                case action_t::no_action:
                    return "NO_ACTION";
                case action_t::restrict:
                    return "RESTRICT";
                case action_t::set_null:
                    return "SET_NULL";
                case action_t::set_default:
                    return "SET_DEFAULT";
                case action_t::cascade:
                    return "CASCADE";
            };

            return "OOPS";
        }

        constexpr const char* conflictStr(conflict_t c) {
            switch(c) {
                case conflict_t::rollback:
                    return "ROLLBACK";
                case conflict_t::abort:
                    return "ABORT";
                case conflict_t::fail:
                    return "FAIL";
                case conflict_t::ignore:
                    return "IGNORE";
                case conflict_t::replace:
                    return "REPLACE";
            };

            return "OOPS";
        }
    }

    template<FixedLengthString tableName, FixedLengthString... column>
        struct Reference {
            static std::string to_string() {
                std::ostringstream ss;
                ss << "REFERENCES `" << tableName.value << "` (`";

                ss << AppendToStream<"`, `", column...>();

                std::string str = ss.str();
                str.erase(str.end() - 3, str.end());
                return str + ")";
            }
        };

    // TODO support defferrable
    template<typename Reference, action_t onUpdate=action_t::no_action, action_t onDelete=action_t::no_action>
    struct ForeignKey {
        static std::string to_string() {
            std::ostringstream ss;
            ss << Reference::to_string()
                << " ON UPDATE " << __constraint_enum_to_str::actionStr(onUpdate)
                << " ON DELETE " << __constraint_enum_to_str::actionStr(onDelete);
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out, [[maybe_unused]] const ForeignKey& c)
        {
            out << to_string();
            return out;
        }
    };

    // TODO support default with expr
    template<FixedLengthString value>
    struct Default {
        static std::string to_string() {
            std::ostringstream ss;
            ss << "DEFAULT '" << value.value << "'";
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out, [[maybe_unused]] const Default& c)
        {
            out << to_string();
            return out;
        }
    };

    template<FixedLengthString value>
    struct Collate {
        static std::string to_string() {
            std::stringstream ss;
            ss << "COLLATE " << value.value;
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out, [[maybe_unused]] const Collate& c)
        {
            out << to_string();
            return out;
        }
    };

    template<FixedLengthString constraint, conflict_t onConflict>
    struct ConstraintWithConflictClause {
        static std::string to_string() {
            std::stringstream ss;
            ss << constraint.value << " ON CONFLICT " << __constraint_enum_to_str::conflictStr(onConflict);
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out,
                [[maybe_unused]] const ConstraintWithConflictClause& c)
        {
            out << to_string();
            return out;
        }
    };

    template<conflict_t onConflict=conflict_t::abort>
    using Unique = ConstraintWithConflictClause<"UNIQUE", onConflict>;

    template<conflict_t onConflict=conflict_t::abort>
    using NotNull = ConstraintWithConflictClause<"NOT NULL", onConflict>;

    template<conflict_t onConflict=conflict_t::abort>
    using PrimaryKey = ConstraintWithConflictClause<"PRIMARY KEY", onConflict>;

    template<conflict_t onConflict=conflict_t::abort>
    using PrimaryKeyAsc = ConstraintWithConflictClause<"PRIMARY KEY ASC", onConflict>;

    template<conflict_t onConflict=conflict_t::abort>
    using PrimaryKeyDesc = ConstraintWithConflictClause<"PRIMARY KEY DESC", onConflict>;

    template <typename T>
    struct ConstraintIsPrimaryKey : std::false_type { };

    template<auto T>
    struct ConstraintIsPrimaryKey<PrimaryKey<T>> : std::true_type {};

    template<auto T>
    struct ConstraintIsPrimaryKey<PrimaryKeyAsc<T>> : std::true_type {};

    template<auto T>
    struct ConstraintIsPrimaryKey<PrimaryKeyDesc<T>> : std::true_type {};

    //TODO add `CHECK` constraint
};
