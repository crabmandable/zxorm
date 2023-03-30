#pragma once
#include "zxorm/zxorm.hpp"

static inline void logger(zxorm::log_level level, const std::string_view& msg) {
    std::cout << "connection logger (" << (int)level << "): ";
    std::cout << msg << std::endl;
}
