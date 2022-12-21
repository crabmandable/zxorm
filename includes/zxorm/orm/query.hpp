#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query_serializer.hpp"
#include "zxorm/orm/clause.hpp"
#include "zxorm/orm/field.hpp"
#include <sqlite3.h>

namespace zxorm {
    template <typename Table>
    class Query {
    protected:
        query_type_t _query_type;
        const char* _table_name = Table::name;
        sqlite3* _handle;
        Logger _logger;
        std::string _join;
        std::shared_ptr<ClauseBase> _where;

        virtual void serialize(std::ostream& ) {}

        Result<Statement> prepare() {
            auto query = QuerySerializer(_query_type, _table_name).serialize([&](std::ostream& ss) {
                if (not _join.empty()) {
                    ss << _join << " ";
                }
                if (_where) {
                    ss << _where->clause << " ";
                }
                serialize(ss);
            });
            ZXORM_GET_RESULT(Statement stmt, Statement::create(_handle, _logger, query));

            if (_where) {
                ZXORM_TRY(_where->bind(stmt));
            }

            return stmt;
        }

    public:
        Query(query_type_t query_type, sqlite3* handle, Logger logger) : _query_type{query_type},  _handle(handle), _logger(logger) {}

        template <typename Expression>
        auto where(const Expression& e) {
            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
            return *this;
        }

        template <Field FieldA, Field FieldB>
        auto join(join_type_t type = join_type_t::INNER) {
            using field_a_t = decltype(FieldA);
            using field_b_t = decltype(FieldB);
            using table_a_t = typename field_a_t::table_t;
            using table_b_t = typename field_b_t::table_t;

            static_assert(std::is_same_v<table_a_t, Table> || std::is_same_v<table_b_t, Table>,
                    "One of the join fields should belong to the table being queried");

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
                _join = serialize(table_b_t::name, field_b_t::name, field_a_t::name);
            } else {
                _join = serialize(table_a_t::name, field_a_t::name, field_b_t::name);
            }

            return *this;
        }
    };
};

