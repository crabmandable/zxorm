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
#include "zxorm/orm/query/prepared_query/prepared_delete_query.hpp"

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
    class DeleteQueryBuilder : public BaseQueryBuilder<
                        std::tuple<__selectable_table<false, From::name>>,
                        From,
                        __delete_detail::DeleteColumnClause>
    {
        using Super = BaseQueryBuilder<
            std::tuple<__selectable_table<false, From::name>>,
            From,
            __delete_detail::DeleteColumnClause>;

    public:
        DeleteQueryBuilder(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        DeleteQueryBuilder(DeleteQueryBuilder&& other) = default;

        template <typename Expression>
        auto where(const Expression& e) -> PreparedDelete<decltype(e.bindings())> {
            Super::where(e);
            Super::prepare();
            return PreparedDelete<decltype(e.bindings())>(Super::_stmt);
        }
    };
};

