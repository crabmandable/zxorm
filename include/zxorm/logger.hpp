/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include <functional>
#include <string_view>

namespace zxorm {
    enum class log_level {
        Error = 0,
        Debug = 1,
    };

    using Logger = std::function<void(log_level, const std::string_view&)>;
};
