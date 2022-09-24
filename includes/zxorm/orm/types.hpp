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
        private:
        static constexpr sql_type_t findType() {
            using type = typename remove_optional<T>::type;
            if constexpr (IsArithmetic<type>()) {
                if (std::is_floating_point_v<type>) {
                    return sql_type_t::REAL;
                } else {
                    return sql_type_t::INTEGER;
                }
            } else if constexpr (IsString<type>()) {
                return sql_type_t::TEXT;
            } else if constexpr (IsContinuousContainer<type>()) {
                return sql_type_t::BLOB;
            } else {
                static_assert(std::is_same_v<T, std::false_type>,
                    "Member type is not convertable to an sql type. "
                    "Please use only arithmetic types or containers with continuous memory, "
                    "or std::optional containing either type");
            }
        }
        public:
        static constexpr sql_type_t value = findType();
    };

};
