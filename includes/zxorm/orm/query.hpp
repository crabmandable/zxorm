#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query_serializer.hpp"
#include "zxorm/orm/where.hpp"
#include <sqlite3.h>

namespace zxorm {
    class Query {
    protected:
        query_type_t _query_type;
        const char* _table_name;
        sqlite3* _handle;
        Logger _logger;
        std::shared_ptr<WhereBase> _where;

        virtual void serialize(std::ostream& ) {}

        Result<Statement> prepare() {
            auto query = QuerySerializer(_query_type, _table_name).serialize([&](std::ostream& ss) {
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
        Query(query_type_t query_type, const char* table_name, sqlite3* handle, Logger logger) : _query_type{query_type}, _table_name{table_name}, _handle(handle), _logger(logger) {}

        template <typename Expression>
        auto where(const Expression& e) {
            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
            return *this;
        }
    };
};

