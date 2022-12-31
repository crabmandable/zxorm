#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query.hpp"

namespace zxorm {
    namespace __delete_detail {
        struct DeleteColumnClause {
            friend std::ostream & operator<< (std::ostream &out, const DeleteColumnClause&) {
                out << "DELETE ";
                return out;
            }
        };
    };
    template <class Table>
    class Delete : public Query<Table, __delete_detail::DeleteColumnClause> {
    using Super = Query<Table, __delete_detail::DeleteColumnClause>;

    public:
        Delete(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        auto where(auto&&... args) {
            Super::where(std::forward<decltype(args)>(args)...);
            return *this;
        }

        template <Field field_a, Field field_b>
        auto join(join_type_t type = join_type_t::INNER) {
            Super::template join<field_a, field_b>(type);
            return *this;
        }

        template <FixedLengthString foreign_table>
        auto join(join_type_t type = join_type_t::INNER) {
            Super::template join<foreign_table>(type);
            return *this;
        }

        OptionalError exec() {
            ZXORM_GET_RESULT(Statement s, Super::prepare());
            return s.step();
        }
    };
};

