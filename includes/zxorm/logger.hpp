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
