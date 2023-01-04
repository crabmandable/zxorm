#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/clause.hpp"
#include "zxorm/orm/field.hpp"
#include <sqlite3.h>

namespace zxorm {
    enum join_type_t {
        INNER,
        OUTER,
        CROSS,
    };

    static inline std::ostream & operator<< (std::ostream &out, const join_type_t& type) {
        switch (type) {
            case join_type_t::INNER:
                out << "INNER JOIN ";
                break;
            case join_type_t::OUTER:
                out << "OUTER JOIN ";
                break;
            case join_type_t::CROSS:
                out << "CROSS JOIN ";
                break;
        };
        return out;
    };

    template <typename Table, typename ColumnClause, typename... JoinedTables>
    class Query {
    protected:
        const char* _table_name = Table::name;
        sqlite3* _handle;
        Logger _logger;
        std::vector<std::string> _join;
        std::shared_ptr<ClauseBase> _where;

        // TODO: this is werid, think of a better way
        virtual void serialize_limits(std::ostream& ) {}

        Result<Statement> prepare() {

            std::stringstream ss;
            ss << ColumnClause();
            ss << "FROM `" << _table_name << "` ";

            if (not _join.empty()) {
                for (const auto& j: _join)
                    ss << j << " ";
            }
            if (_where) {
                ss << _where->clause << " ";
            }

            serialize_limits(ss);

            ss << ";";

            ZXORM_GET_RESULT(Statement stmt, Statement::create(_handle, _logger, ss.str()));

            if (_where) {
                ZXORM_TRY(_where->bind(stmt));
            }

            return stmt;
        }

    public:
        Query(sqlite3* handle, Logger logger) : _handle(handle), _logger(logger) {}

        template <typename Expression>
        auto where(const Expression& e) {
            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
            return *this;
        }

        template <FixedLengthString foreign_table>
        auto join(join_type_t type = join_type_t::INNER) {
            static_assert(Table::template does_reference_table<foreign_table>,
                    "The selected table does not contain a foreign key referencing this table");

            using select_column = typename Table::foreign_column<foreign_table>;
            using foreign_key_t = typename select_column::foreign_key_t;

            std::ostringstream ss;
            ss << type << "`" << foreign_key_t::table_name.value << "`"
                << " ON `" << _table_name << "`.`" << select_column::name.value << "`"
                << " = `" << foreign_key_t::table_name.value << "`.`" << foreign_key_t::column_name.value << "`";

            _join.push_back(ss.str());

            return *this;
        }

        template <Field FieldA, Field FieldB>
        auto join(join_type_t type = join_type_t::INNER) {
            using field_a_t = decltype(FieldA);
            using field_b_t = decltype(FieldB);
            using table_a_t = typename field_a_t::table_t;
            using table_b_t = typename field_b_t::table_t;

            static_assert(std::is_same_v<table_a_t, Table> || std::is_same_v<table_b_t, Table> ||
                    any_of<std::is_same_v<table_a_t, JoinedTables>...> || any_of<std::is_same_v<table_b_t, JoinedTables>...>,
                    "One of the join fields should belong to the table being queried, "
                    "or another table already joined");

            // using this lambda to get arround the fact that we have a constexpr if
            // which means I can't just make the join_table, join_field and select_fields variables,
            // since they're const and have to be initialized on declaration
            // pretty lame...
            auto serialize = [&](const char* join_table, const char* join_field, const char* select_field) {
                std::ostringstream ss;
                ss << type << "`" << join_table << "`"
                    << " ON `" << _table_name << "`.`" << select_field << "`"
                    << " = `" << join_table << "`.`" << join_field << "`";
                return ss.str();
            };

            if constexpr (std::is_same_v<table_a_t, Table>) {
                _join.push_back(serialize(table_b_t::name, field_b_t::name, field_a_t::name));
            } else {
                _join.push_back(serialize(table_a_t::name, field_a_t::name, field_b_t::name));
            }

            return *this;
        }
    };
};

