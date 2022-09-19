#pragma once

#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include <cassert>
#include <cstring>
#include <iterator>
#include <optional>
#include <type_traits>

namespace zxorm {
    template <class Connection, class... Table>
    class Statement {
        public:
        std::optional<Error> error = std::nullopt;

        Statement(Connection* conn, const std::string& query) : conn{conn} {
            conn->_logger(LogLevel::Debug, "Prepared statement");
            conn->_logger(LogLevel::Debug, query.c_str());

            int result = sqlite3_prepare_v2(conn->_db_handle, query.c_str(), query.size() + 1, &stmt, nullptr);
            if (result != SQLITE_OK) {
                const char* str = sqlite3_errstr(result);
                conn->_logger(LogLevel::Error, "Unable to initialize statment");
                conn->_logger(LogLevel::Error, str);
                error = Error("Unable to initialize statement", result);
                return;
            }

            parameterCount = sqlite3_bind_parameter_count(stmt);
            for (size_t i = 1; i == parameterCount; i++) {
                isBound[i] = false;
            }
        }

        ~Statement() {
            int result = sqlite3_finalize(stmt);
            if (result != SQLITE_OK) {
                const char* str = sqlite3_errstr(result);
                conn->_logger(LogLevel::Error, "Unable to finalize statment");
                conn->_logger(LogLevel::Error, str);
            }
        }

        private:
        friend Connection;
        Connection* conn;
        sqlite3_stmt* stmt;

        size_t parameterCount;
        std::map<size_t, bool> isBound;
        int columnCount = 0;
        bool done = false;


        template <typename T>
        std::optional<Error> bind(size_t idx, const T& param)
        requires (std::is_arithmetic_v<T>)
        {
            int result;
            if constexpr (std::is_floating_point_v<T>) {
                result = sqlite3_bind_double(stmt, idx, param);
            } else {
                if (sizeof(T) <= 4) {
                    result = sqlite3_bind_int(stmt, idx, param);
                } else {
                    result = sqlite3_bind_int64(stmt, idx, param);
                }
            }
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement");
                return Error("Unable to bind parameter to statment", result);
            }
            isBound[idx] = true;
            return std::nullopt;
        }

        std::optional<Error> bind(size_t idx, const void* data, size_t len) {
            int result = sqlite3_bind_blob(stmt, idx, data, len, nullptr);
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement");
                return Error("Unable to bind parameter to statment", result);
            }
            std::cout << "Bind " << len << "\n";
            isBound[idx] = true;
            return std::nullopt;
        }

        std::optional<Error> bind(const char* paramName, const void* data, size_t len) {
            int idx = sqlite3_bind_parameter_index(stmt, paramName);
            if (idx <= 0) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement, name not found");
                return Error("Unable to bind parameter to statment, name not found");
            }
            return bind(idx, data, len);
        }

        std::optional<Error> rewind() {
            int result = sqlite3_reset(stmt);
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to reset statment");
                return Error( "Unable to reset statment", result);
            }
            done = false;
        }

        std::optional<Error> reset() {
            auto err = rewind();
            if (err) return err;

            int result = sqlite3_clear_bindings(stmt);
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to clear bindings");
                return Error( "Unable to clear bindings", result);
            }
            for (auto& [_, v] : isBound) v = false;

            return std::nullopt;
        }

        std::optional<Error> step() {
            if (done) {
                return Error("Query has run to completion");
            }
            if (std::any_of(isBound.begin(), isBound.end(),
                        [](const auto& bound) { return !bound.second; } )) {
                return Error("Some parameters have not been bound");
            }

            int result = sqlite3_step(stmt);
            if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
                conn->_logger(LogLevel::Error, "Unable to execute statment");
                return Error("Unable to execute statment", result);
            }

            done = result == SQLITE_DONE;

            columnCount = sqlite3_column_count(stmt);

            return std::nullopt;
        }

        template<typename T>
        void readColumn(size_t idx, T& value) {
            int dataType = sqlite3_column_type(stmt, idx);
            switch(dataType) {
                case SQLITE_INTEGER: {
                    if (sizeof(value) <= 4) {
                        value = sqlite3_column_int(stmt, idx);
                    } else {
                        value = sqlite3_column_int64(stmt, idx);
                    }
                    break;
                }
                case SQLITE_FLOAT: {
                    value = sqlite3_column_double(stmt, idx);
                    break;
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    const void* data = sqlite3_column_blob(stmt, idx);
                    size_t len = sqlite3_column_bytes(stmt, idx);
                    if constexpr (BackInsertable<decltype(value)>) {
                        std::copy(data, (uint8_t*)data + len, std::back_inserter(value));
                    } else if constexpr (std::has_unique_object_representations_v<T> && std::is_trivial_v<T>) {
                        size_t toCopy = std::min(sizeof(T),  len);
                        std::memcpy(&value, data, toCopy);
                    } else {
                        static_assert(std::is_same_v<typename std::type_identity<T>::type, T>,
                                "Type of column passed to read column doesn't make sense to read from a blob");
                    }
                    break;
                }
                case SQLITE_NULL: {
                    if constexpr (requires { value.clear(); }) {
                        value.clear();
                    }
                    break;
                }
                default: {
                    // this should never happen :pray:
                    assert(false);
                    break;
                }
            }
        }
    };
};
