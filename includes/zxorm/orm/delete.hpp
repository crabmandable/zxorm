#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query.hpp"

namespace zxorm {
    template <class Table>
    class Delete : public Query {

    public:
        Delete(sqlite3* handle, Logger logger) :
            Query(query_type_t::DELETE, Table::name, handle, logger) {}

        auto where(auto&&... args) {
            Query::where(std::forward<decltype(args)>(args)...);
            return *this;
        }

        OptionalError exec() {
            ZXORM_GET_RESULT(Statement s, prepare());
            return s.step();
        }
    };
};

