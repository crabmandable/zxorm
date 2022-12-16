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
        constexpr const char* action_str(action_t c) {
            switch(c) {
                case action_t::no_action:
                    return "NO ACTION";
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

        constexpr const char* conflict_str(conflict_t c) {
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

    template<FixedLengthString table_name, FixedLengthString... column>
        struct Reference {
            static std::string to_string() {
                std::ostringstream ss;
                ss << "REFERENCES `" << table_name.value << "` (`";

                ss << append_to_stream<"`, `", column...>();

                std::string str = ss.str();
                str.erase(str.end() - 3, str.end());
                return str + ")";
            }
        };

    // TODO support defferrable
    template<typename Reference, action_t on_update=action_t::no_action, action_t on_delete=action_t::no_action>
    struct ForeignKey {
        static std::string to_string() {
            std::ostringstream ss;
            ss << Reference::to_string()
                << " ON UPDATE " << __constraint_enum_to_str::action_str(on_update)
                << " ON DELETE " << __constraint_enum_to_str::action_str(on_delete);
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

    template<FixedLengthString constraint, conflict_t on_conflict>
    struct ConstraintWithConflictClause {
        static std::string to_string() {
            std::stringstream ss;
            ss << constraint.value << " ON CONFLICT " << __constraint_enum_to_str::conflict_str(on_conflict);
            return ss.str();
        }

        friend std::ostream & operator<< (std::ostream &out,
                [[maybe_unused]] const ConstraintWithConflictClause& c)
        {
            out << to_string();
            return out;
        }
    };

    template<conflict_t on_conflict=conflict_t::abort>
    using Unique = ConstraintWithConflictClause<"UNIQUE", on_conflict>;

    template<conflict_t on_conflict=conflict_t::abort>
    using NotNull = ConstraintWithConflictClause<"NOT NULL", on_conflict>;

    template<conflict_t on_conflict=conflict_t::abort>
    using PrimaryKey = ConstraintWithConflictClause<"PRIMARY KEY", on_conflict>;

    template<conflict_t on_conflict=conflict_t::abort>
    using PrimaryKeyAsc = ConstraintWithConflictClause<"PRIMARY KEY ASC", on_conflict>;

    template<conflict_t on_conflict=conflict_t::abort>
    using PrimaryKeyDesc = ConstraintWithConflictClause<"PRIMARY KEY DESC", on_conflict>;

    template <typename T>
    struct constraint_is_primary_key : std::false_type { };

    template<auto T>
    struct constraint_is_primary_key<PrimaryKey<T>> : std::true_type {};

    template<auto T>
    struct constraint_is_primary_key<PrimaryKeyAsc<T>> : std::true_type {};

    template<auto T>
    struct constraint_is_primary_key<PrimaryKeyDesc<T>> : std::true_type {};

    //TODO add `CHECK` constraint
};
