#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query.hpp"
#include "zxorm/orm/record_iterator.hpp"

namespace zxorm {
    template <class Table>
    class Select : public Query {
    private:

        std::string _limit_clause;
        std::string _order_clause;

        virtual void serialize(std::ostream& ss) override {
            ss << _order_clause << " " << _limit_clause;
        }

    public:
        Select(sqlite3* handle, Logger logger) :
            Query(query_type_t::SELECT, Table::name, handle, logger) {}

        auto where(auto&&... args) {
            Query::where(std::forward<decltype(args)>(args)...);
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
