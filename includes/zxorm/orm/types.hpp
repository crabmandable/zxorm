#pragma once
#include <string>

namespace zxorm {

enum class sql_type_t {
    INTEGER,
    TEXT,
    BLOB,
    REAL,
    NUMERIC,
};

static constexpr const char* sqlTypeStr(sql_type_t t) {
    switch(t) {
        case sql_type_t::INTEGER: {
            return "INTEGER";
        }
        case sql_type_t::TEXT: {
            return "TEXT";
        }
        case sql_type_t::BLOB: {
            return "BLOB";
        }
        case sql_type_t::REAL: {
            return "REAL";
        }
        case sql_type_t::NUMERIC: {
            return "NUMERIC";
        }
    }

    return "OOPS";
}

enum class order_t {
    NONE,
    ASC,
    DESC,
};

static constexpr const char* orderStr(order_t t) {
    switch(t) {
        case order_t::NONE: {
            return "";
        }
        case order_t::ASC: {
            return "ASC";
        }
        case order_t::DESC: {
            return "DESC";
        }
    }

    return "OOPS";
}

template<typename T>
struct MemberTypeToSQLType {
    static constexpr sql_type_t value = sql_type_t::BLOB;
};

#define DEFINE_MEMBER_TO_SQL_TYPE(memberType, sqlType) \
template<> \
struct MemberTypeToSQLType<memberType> { \
    static constexpr const sql_type_t value = sql_type_t::sqlType; \
};

DEFINE_MEMBER_TO_SQL_TYPE(int, INTEGER)
DEFINE_MEMBER_TO_SQL_TYPE(float, REAL)
DEFINE_MEMBER_TO_SQL_TYPE(bool, INTEGER)
DEFINE_MEMBER_TO_SQL_TYPE(void*, BLOB)
DEFINE_MEMBER_TO_SQL_TYPE(std::string, TEXT)
#undef DEFINE_MEMBER_TO_SQL_TYPE

};

