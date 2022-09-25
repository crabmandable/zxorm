#pragma once
#include <string>

namespace zxorm {

    enum class sqlite_column_type {
        INTEGER = SQLITE_INTEGER,
        TEXT = SQLITE_TEXT,
        BLOB = SQLITE_BLOB,
        REAL = SQLITE_FLOAT,
    };

    static constexpr const char* sqlTypeStr(sqlite_column_type t) {
        switch(t) {
            case sqlite_column_type::INTEGER: {
                return "INTEGER";
            }
            case sqlite_column_type::TEXT: {
                return "TEXT";
            }
            case sqlite_column_type::BLOB: {
                return "BLOB";
            }
            case sqlite_column_type::REAL: {
                return "REAL";
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
        static constexpr sqlite_column_type findType() {
            using type = typename remove_optional<T>::type;
            if constexpr (IsArithmetic<type>()) {
                if (std::is_floating_point_v<type>) {
                    return sqlite_column_type::REAL;
                } else {
                    return sqlite_column_type::INTEGER;
                }
            } else if constexpr (IsString<type>()) {
                return sqlite_column_type::TEXT;
            } else if constexpr (IsContinuousContainer<type>()) {
                return sqlite_column_type::BLOB;
            } else {
                static_assert(std::is_same_v<T, std::false_type>,
                    "Member type is not convertable to an sql type. "
                    "Please use only arithmetic types or containers with continuous memory, "
                    "or std::optional containing either type");
            }
        }
        public:
        static constexpr sqlite_column_type value = findType();
    };

};
