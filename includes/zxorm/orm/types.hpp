/*
* Copyright (c) 2023 Zach Gerstman
* https://github.com/crabmandable/zxorm
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/
#pragma once
#include <string>

namespace zxorm {

    enum class sqlite_column_type {
        INTEGER = SQLITE_INTEGER,
        TEXT = SQLITE_TEXT,
        BLOB = SQLITE_BLOB,
        REAL = SQLITE_FLOAT,
    };

    static constexpr const char* sql_type_str(sqlite_column_type t) {
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
        ASC,
        DESC,
    };

    static inline std::ostream & operator<< (std::ostream &out, const order_t& type) {
        switch(type) {
            case order_t::ASC: {
                out << "ASC";
                break;
            }
            case order_t::DESC: {
                out << "DESC";
                break;
            }
        }

        return out;
    }

    template<typename T>
    struct member_to_sql_type {
        private:
        static constexpr sqlite_column_type _impl() {
            if constexpr (ignore_qualifiers::is_arithmetic<T>()) {
                if (ignore_qualifiers::is_floating_point<T>()) {
                    return sqlite_column_type::REAL;
                } else {
                    return sqlite_column_type::INTEGER;
                }
            } else if constexpr (ignore_qualifiers::is_string<T>()) {
                return sqlite_column_type::TEXT;
            } else if constexpr (ignore_qualifiers::is_continuous_container<T>()) {
                return sqlite_column_type::BLOB;
            } else if constexpr (std::is_convertible_v<T, sqlite3_int64>) {
                return sqlite_column_type::INTEGER;
            } else if constexpr (std::is_convertible_v<T, double>) {
                return sqlite_column_type::REAL;
            } else {
                static_assert(std::is_same_v<T, std::false_type>,
                    "Member type is not convertable to an sql type. "
                    "Please use only arithmetic types or containers with continuous memory, "
                    "or std::optional containing either type");
            }
        }
        public:
        static constexpr sqlite_column_type value = _impl();
    };

};
