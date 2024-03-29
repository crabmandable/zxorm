/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include <memory>
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/orm/query/builder/base_query_builder.hpp"
#include "zxorm/orm/query/prepared_query/prepared_select_query.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/table.hpp"

namespace zxorm {
    template <typename SelectablesTuple, typename Select, typename JoinsTuple=std::tuple<>>
    class SelectQueryBuilder : public BaseQueryBuilder<SelectablesTuple, typename Select::from_t, Select, JoinsTuple> {
    private:
        using Super = BaseQueryBuilder<SelectablesTuple, typename Select::from_t, Select, JoinsTuple>;

        std::string _limit_clause;
        std::string _order_clause;
        std::string _group_by_clause;

        virtual void serialize_limits(std::ostream& ss) override {
            ss << _group_by_clause << " " << _order_clause << " " << _limit_clause;
        }

    public:
        SelectQueryBuilder(sqlite3* handle, std::weak_ptr<Logger> logger) :
            Super(handle, logger) {}

        SelectQueryBuilder(SelectQueryBuilder&& other) = default;

        auto& limit(unsigned long limit, unsigned long offset = 0) {
            if (_limit_clause.empty()) {
                std::stringstream ss;
                ss << "LIMIT " << limit;
                if (offset) {
                    ss << " OFFSET " << offset;
                }
                _limit_clause = ss.str();
            }
            return *this;
        }

        template <typename Field>
        requires(is_field<Field>)
        auto& order_by(order_t ord = order_t::ASC) {
            if (_order_clause.empty()) {
                using column_t = typename Field::column_t;
                using table_t = typename Field::table_t;

                static_assert(table_is_selectable<table_t, SelectablesTuple>::value,
                        "Field for `order_by` must belong to a table present in the query");

                std::stringstream ss;
                ss << "ORDER BY `" << table_t::name.value << "`.`" << column_t::name.value << "` " << ord;
                _order_clause = ss.str();
            }
            return *this;
        }

        template <typename T>
        auto& group_by() {
            if (_group_by_clause.empty()) {
                const char* column_name;
                const char* table_name;

                static_assert(is_field<T> || is_table_v<T>,
                        "Argument for `group_by` should be a Field or a Table");

                if constexpr (is_field<T>) {
                    column_name = T::column_t::name.value;
                    table_name = T::table_t::name.value;
                    static_assert(table_is_selectable<typename T::table_t, SelectablesTuple>::value,
                            "Field for `group_by` must belong to a table present in the query");
                } else {
                    column_name = T::primary_key_t::name.value;
                    table_name = T::name.value;
                    static_assert(table_is_selectable<T, SelectablesTuple>::value,
                            "Table for `group_by` must be a table present in the query");
                }


                std::stringstream ss;
                ss << "GROUP BY `" << table_name << "`.`" << column_name << "`";
                _group_by_clause = ss.str();
            }
            return *this;
        }

        template <typename Expression>
        auto where_one(const Expression& e) -> PreparedSelectOne<Select, decltype(e.bindings())>
        {
            Super::where(e);
            limit(1);
            Super::prepare();
            return PreparedSelectOne<Select, decltype(e.bindings())>(Super::_stmt);
        }

        template <typename Expression>
        auto where_many(const Expression& e) -> PreparedSelectMany<Select, decltype(e.bindings())>
        {
            Super::where(e);
            Super::prepare();
            return PreparedSelectMany<Select, decltype(e.bindings())>(Super::_stmt);
        }

        auto one() -> PreparedSelectOne<Select>
        {
            limit(1);
            Super::prepare();
            return PreparedSelectOne<Select>(Super::_stmt);
        }

        auto many() -> PreparedSelectMany<Select>
        {
            Super::prepare();
            return PreparedSelectMany<Select>(Super::_stmt);
        }
    };
};
