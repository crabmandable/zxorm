/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include "zxorm/common.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/query/clause.hpp"
#include <sqlite3.h>

namespace zxorm {
    template <typename SelectablesTuple, typename Table, typename ColumnClause, typename JoinsTuple=std::tuple<>>
    class BaseQueryBuilder {
    protected:
        const char* _table_name = Table::name.value;
        sqlite3* _handle;
        Logger _logger;
        std::shared_ptr<BindingClauseBase> _where;
        std::shared_ptr<Statement> _stmt;

        // TODO: this is werid, think of a better way
        virtual void serialize_limits(std::ostream& ) {}

        void prepare()
        {
            if (!_stmt) {
                std::stringstream ss;
                ss << ColumnClause();
                ss << "FROM `" << _table_name << "` ";

                std::apply([&](const auto&... j) {
                        ((ss << j << " "), ...);
                        }, JoinsTuple{});

                if (_where) {
                    ss << _where->clause << " ";
                }

                serialize_limits(ss);

                ss << ";";

                _stmt = std::make_shared<Statement>(_handle, _logger, ss.str());
            } else {
                _stmt->reset();
            }

            if (_where) {
                _where->bind(*_stmt);
            }
        }

        template <typename Expression>
        void where(const Expression& e) {

            static_assert(tables_are_selectable<typename Expression::tables_t, SelectablesTuple>::value,
                    "Some fields in `Where` expression are not present in the query");

            _where = std::make_shared<Where<decltype(e.bindings())>>(e);
        }

        BaseQueryBuilder(sqlite3* handle, Logger logger) : _handle(handle), _logger(logger) { }

        BaseQueryBuilder(BaseQueryBuilder&& old) = default;
        BaseQueryBuilder& operator=(BaseQueryBuilder&& old) = default;

        BaseQueryBuilder(const BaseQueryBuilder& old) = delete;
        BaseQueryBuilder& operator=(const BaseQueryBuilder& old) = delete;

    public:
        virtual ~BaseQueryBuilder() = default;
    };
};

