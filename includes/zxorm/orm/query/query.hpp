#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query/clause.hpp"
#include "zxorm/orm/field.hpp"
#include <sqlite3.h>

namespace zxorm {

    template <typename Table, typename ColumnClause, typename... Joins>
    class Query {
    protected:
        const char* _table_name = Table::name.value;
        sqlite3* _handle;
        Logger _logger;
        std::shared_ptr<BindingClauseBase> _where;

        // TODO: this is werid, think of a better way
        virtual void serialize_limits(std::ostream& ) {}

        Result<Statement> prepare() {

            std::stringstream ss;
            ss << ColumnClause();
            ss << "FROM `" << _table_name << "` ";

            ((ss << Joins{} << " "), ...);

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
    };
};

