#pragma once
#include "zxorm/common.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query/clause.hpp"
#include <sqlite3.h>

namespace zxorm {
    class BaseQuery {
    public:
        ~BaseQuery() = default;

    protected:
        BaseQuery() = default;

        BaseQuery(BaseQuery&& old) = default;
        BaseQuery& operator=(BaseQuery&& old) = default;

        BaseQuery(const BaseQuery& old) = delete;
        BaseQuery& operator=(const BaseQuery& old) = delete;
    };

    class BasePreparedQuery : public BaseQuery {
    public:
        BasePreparedQuery() = default;
        BasePreparedQuery(BasePreparedQuery&& old) = default;
    };

    template <typename SelectablesTuple, typename Table, typename ColumnClause, typename JoinsTuple=std::tuple<>>
    class Query : public BaseQuery {
    protected:
        const char* _table_name = Table::name.value;
        sqlite3* _handle;
        Logger _logger;
        std::shared_ptr<BindingClauseBase> _where;
        std::shared_ptr<Statement> _stmt;

        // TODO: this is werid, think of a better way
        virtual void serialize_limits(std::ostream& ) {}

        void prepare()
        {
            if (!_stmt) {
                std::stringstream ss;
                ss << ColumnClause();
                ss << "FROM `" << _table_name << "` ";

                std::apply([&](const auto&... j) {
                        ((ss << j << " "), ...);
                        }, JoinsTuple{});

                if (_where) {
                    ss << _where->clause << " ";
                }

                serialize_limits(ss);

                ss << ";";

                _stmt = std::make_shared<Statement>(_handle, _logger, ss.str());
            } else {
                _stmt->reset();
            }

            if (_where) {
                _where->bind(*_stmt);
            }
        }

    public:
        Query(sqlite3* handle, Logger logger) : _handle(handle), _logger(logger) { }
        Query(Query&&other) = default;
        virtual ~Query() = default;

        template <typename Expression>
        void where(const Expression& e) {

            static_assert(tables_are_selectable<typename Expression::tables_t, SelectablesTuple>::value,
                    "Some fields in `Where` expression are not present in the query");

            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
        }
    };
};

