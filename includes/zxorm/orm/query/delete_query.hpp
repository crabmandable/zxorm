#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query/query.hpp"

namespace zxorm {
    namespace __delete_detail {
        struct DeleteColumnClause {
            friend std::ostream & operator<< (std::ostream &out, const DeleteColumnClause&) {
                out << "DELETE ";
                return out;
            }
        };
    };
    template <class From>
    class DeleteQuery : public Query<From, __delete_detail::DeleteColumnClause> {
    using Super = Query<From, __delete_detail::DeleteColumnClause>;

    public:
        DeleteQuery(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        auto where(auto&&... args) {
            Super::where(std::forward<decltype(args)>(args)...);
            return *this;
        }

        OptionalError exec() {
            ZXORM_GET_RESULT(Statement s, Super::prepare());
            return s.step();
        }
    };
};

