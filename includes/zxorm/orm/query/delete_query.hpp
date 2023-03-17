#pragma once
#include <memory>
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query/query.hpp"

namespace zxorm {
    template <class Bindings = std::tuple<>>
    class PreparedDelete: public BasePreparedQuery {
        std::shared_ptr<Statement> _stmt;
        Bindings _bindings;
    public:
        PreparedDelete(std::shared_ptr<Statement> stmt) : _stmt(stmt) {}
        PreparedDelete(PreparedDelete&&) = default;

        OptionalError rebind(auto&&... bindings) {
            _bindings = Bindings{std::forward<decltype(bindings)>(bindings)...};

            ZXORM_TRY(_stmt->reset());
            ZXORM_TRY(_stmt->bind(_bindings));
            return std::nullopt;
        }

        OptionalError exec() {
            ZXORM_TRY(_stmt->rewind());
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
        auto where(const Expression& e) -> Result<PreparedDelete<decltype(e.bindings())>> {
            Super::where(e);
            ZXORM_TRY(Super::prepare());
            return PreparedDelete<decltype(e.bindings())>(Super::_stmt);
        }

        auto prepare() {
            ZXORM_TRY(Super::prepare());
            return PreparedDelete<>(Super::_stmt);
        }
    };
};

