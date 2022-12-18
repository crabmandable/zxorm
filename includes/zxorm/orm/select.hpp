#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query_serializer.hpp"
#include "zxorm/orm/where.hpp"
#include <sqlite3.h>

namespace zxorm {
    template <class Table, typename Bindings = std::tuple<>>
    class Select {
    private:
        using binder_t = std::function<OptionalError(Statement&)>;
        sqlite3* _handle;
        Logger _logger;
        std::shared_ptr<WhereBase> _where;
        std::string _limit_clause;
        std::string _order_clause;

        Result<Statement> prepare() {
            auto query = QuerySerializer(query_type_t::SELECT, Table::name).serialize([&](std::ostream& ss) {
                if (_where) {
                    ss << _where->clause << " ";
                }
                ss << _order_clause << " " << _limit_clause;
            });
            ZXORM_GET_RESULT(Statement stmt, Statement::create(_handle, _logger, query));

            if (_where) {
                ZXORM_TRY(_where->bind(stmt));
            }

            return stmt;
        }

    public:
        Select(sqlite3* handle, Logger logger) : _handle(handle), _logger(logger) {}

        template <typename Expression>
        auto where(const Expression& e) {
            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
            return *this;
        }

        auto limit(unsigned long limit) {
            std::stringstream ss;
            ss << "LIMIT " << limit;
            _limit_clause = ss.str();
            return *this;
        }

        template <FixedLengthString field>
        auto order_by(order_t ord) {
            static_assert(not std::is_same_v<typename Table::column_by_name<field>::type, std::false_type>,
                "ORDER BY clause must use a field beloning to the Table"
            );
            std::stringstream ss;
            ss << "ORDER BY `" << field.value << "` " << ord;
            _order_clause = ss.str();
            return *this;
        }

        OptionalResult<typename Table::object_class> one() {
            assert(_limit_clause.empty());
            limit(1);
            ZXORM_GET_RESULT(Statement s, prepare());
            ZXORM_TRY(s.step());
            if (s.done()) {
                return std::nullopt;
            }
            return Table::get_row(s);
        }

        Result<RecordIterator<Table>> many() {
            auto s = prepare();
            if (s.is_error()) {
                return s.error();
            }
            return RecordIterator<Table>(std::move(s.value()));
        }
    };
};
