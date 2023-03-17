#pragma once
#include <memory>
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/orm/query/query.hpp"

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

    namespace __delete_detail {
        struct DeleteColumnClause {
            friend std::ostream & operator<< (std::ostream &out, const DeleteColumnClause&) {
                out << "DELETE ";
                return out;
            }
        };
    };
    template <class From>
    class DeleteQuery : public Query<
                        std::tuple<__selectable_table<false, From::name>>,
                        From,
                        __delete_detail::DeleteColumnClause>
    {
        using Super = Query<
            std::tuple<__selectable_table<false, From::name>>,
            From,
            __delete_detail::DeleteColumnClause>;

    public:
        DeleteQuery(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        DeleteQuery(DeleteQuery&& other) = default;

        template <typename Expression>
        auto where(const Expression& e) -> PreparedDelete<decltype(e.bindings())> {
            Super::where(e);
            Super::prepare();
            return PreparedDelete<decltype(e.bindings())>(Super::_stmt);
        }
    };
};

