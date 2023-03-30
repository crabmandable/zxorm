/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once

namespace zxorm {
    class BasePreparedQuery {
    public:
        ~BasePreparedQuery() = default;
    protected:
        BasePreparedQuery() = default;

        BasePreparedQuery(BasePreparedQuery&& old) = default;
        BasePreparedQuery& operator=(BasePreparedQuery&& old) = default;

        BasePreparedQuery(const BasePreparedQuery& old) = delete;
        BasePreparedQuery& operator=(const BasePreparedQuery& old) = delete;
    };
};
