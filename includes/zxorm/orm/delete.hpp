#pragma once
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/query_serializer.hpp"
#include <sqlite3.h>

namespace zxorm {
    template <class Table, typename Bindings = std::tuple<>>
    class Delete {
    private:
        sqlite3* _handle;
        Logger _logger;
        Bindings _bindings;
        std::string _where_clause;

        Result<Statement> prepare() {
            auto query = QuerySerializer(query_type_t::DELETE, Table::name).serialize([&](std::ostream& ss) {
                ss << _where_clause;
            });
            ZXORM_GET_RESULT(Statement stmt, Statement::create(_handle, _logger, query));
            auto err = stmt.bind(_bindings);
            if (err) return err.value();

            return stmt;
        }

    public:
        Delete(sqlite3* handle, Logger logger, std::string where_clause, Bindings bindings):
            _handle(handle), _logger(logger), _bindings(bindings), _where_clause(std::string("WHERE ") + where_clause) {}

        Delete(sqlite3* handle, Logger logger) : _handle(handle), _logger(logger) {}

        OptionalError exec() {
            ZXORM_GET_RESULT(Statement s, prepare());
            return s.step();
        }
    };
};

