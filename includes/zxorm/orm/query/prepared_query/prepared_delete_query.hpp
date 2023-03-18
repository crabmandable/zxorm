#pragma once


#include <memory>
#include <optional>
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/query/prepared_query/base_prepared_query.hpp"

namespace zxorm {
    template <class Bindings = std::tuple<>>
    class PreparedDelete: public BasePreparedQuery {
        std::shared_ptr<Statement> _stmt;
        Bindings _bindings;
    public:
        PreparedDelete(std::shared_ptr<Statement> stmt) : _stmt(stmt) {}
        PreparedDelete(PreparedDelete&&) = default;

        void rebind(auto&&... bindings) {
            _bindings = Bindings{std::forward<decltype(bindings)>(bindings)...};

            _stmt->reset();
            _stmt->bind(_bindings);
        }

        void exec() {
            _stmt->rewind();
            return _stmt->step();
        }
    };
};
