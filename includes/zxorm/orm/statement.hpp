#pragma once

#include "zxorm/common.hpp"
#include "zxorm/orm/types.hpp"
#include <cassert>
#include <cstring>
#include <iterator>
#include <optional>
#include <type_traits>
#include "zxorm/helpers/meta_container.hpp"

namespace zxorm {
    template <class Connection, class... Table>
    class Statement {
        public:
        std::optional<Error> error = std::nullopt;

        Statement(Connection* conn, const std::string& query) : conn{conn} {
            conn->_logger(LogLevel::Debug, "Prepared statement");
            conn->_logger(LogLevel::Debug, query.c_str());

            int result = sqlite3_prepare_v2(conn->_db_handle.get(), query.c_str(), query.size() + 1, &stmt, nullptr);
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


        template <ArithmeticT T>
        [[nodiscard]] std::optional<Error> bind(size_t idx, const T& param)
        {
            using unwrapped_t = typename remove_optional<T>::type;
            const unwrapped_t* unwrapped;
            int result;
            bool boundNull = false;
            if constexpr (IsOptional<T>()) {
                if (!param.has_value()) {
                    result = sqlite3_bind_null(stmt, idx);
                    boundNull = true;
                } else {
                    unwrapped = &param.value();
                }
            } else {
                unwrapped = &param;
            }

            if (!boundNull) {
                if constexpr (std::is_floating_point_v<unwrapped_t>) {
                    result = sqlite3_bind_double(stmt, idx, *unwrapped);
                } else {
                    if (sizeof(T) <= 4) {
                        result = sqlite3_bind_int(stmt, idx, *unwrapped);
                    } else {
                        result = sqlite3_bind_int64(stmt, idx, *unwrapped);
                    }
                }
            }

            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement");
                return Error("Unable to bind parameter to statment", result);
            }
            isBound[idx] = true;
            return std::nullopt;
        }

        template <ContinuousContainer T>
        [[nodiscard]] std::optional<Error> bind(size_t idx, const T& param)
        {
            bool boundNull = false;
            int result;

            if constexpr (IsOptional<T>()) {
                if (!param.has_value()) {
                    result = sqlite3_bind_null(stmt, idx);
                    boundNull = true;
                }
            }

            if (!boundNull) {
                const auto paramToBind = MetaContainer<const T>(param);
                constexpr size_t elSize = sizeof(typename remove_optional<T>::type::value_type);
                result = sqlite3_bind_blob(stmt, idx, paramToBind.data(), paramToBind.size() * elSize, nullptr);
            }

            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to bind parameter to statement");
                return Error("Unable to bind parameter to statment", result);
            }

            isBound[idx] = true;

            return std::nullopt;
        }

        [[nodiscard]] std::optional<Error> rewind() {
            int result = sqlite3_reset(stmt);
            if (result != SQLITE_OK) {
                conn->_logger(LogLevel::Error, "Unable to reset statment");
                return Error( "Unable to reset statment", result);
            }
            done = false;
        }

        [[nodiscard]] std::optional<Error> reset() {
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

        [[nodiscard]] std::optional<Error> step() {
            if (done) {
                return Error("Query has run to completion");
            }
            if (std::any_of(isBound.cbegin(), isBound.cend(),
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

        template<ContinuousContainer T>
        [[nodiscard]] std::optional<Error> readColumn(size_t idx, T& outParam) {
            auto out = MetaContainer<T>(outParam);
            int dataType = sqlite3_column_type(stmt, idx);
            switch(dataType) {
                case SQLITE_INTEGER:
                case SQLITE_FLOAT: {
                    return Error("Tried to read an arithmetic column into a container");
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    const void* data = sqlite3_column_blob(stmt, idx);
                    size_t len = sqlite3_column_bytes(stmt, idx);
                    if (!out.resize(len)) {
                        return Error("Container does not have enough space to read column data");
                    }
                    std::memcpy(out.data(), data, len);
                    break;
                }
                case SQLITE_NULL: {
                    out.clear();
                    break;
                }
                default: {
                    // this should never happen :pray:
                    assert(false);
                    return Error("Unknown SQL type encountered, something isn't implemented yet");
                }
            }
            return std::nullopt;
        }

        template<ArithmeticT T>
        [[nodiscard]] std::optional<Error> readColumn(size_t idx, T& outParam) {
            int dataType = sqlite3_column_type(stmt, idx);
            switch(dataType) {
                case SQLITE_INTEGER: {
                    if (sizeof(outParam) <= 4) {
                        outParam = sqlite3_column_int(stmt, idx);
                    } else {
                        outParam = sqlite3_column_int64(stmt, idx);
                    }
                    break;
                }
                case SQLITE_FLOAT: {
                    outParam = sqlite3_column_double(stmt, idx);
                    break;
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    return Error("Tried to read an arithmetic column into a container");
                    size_t len = sqlite3_column_bytes(stmt, idx);
                    if (len > sizeof(T)) {
                        return Error("Param does not have enough space to fit column contents");
                    }
                    const void* data = sqlite3_column_blob(stmt, idx);
                    std::memcpy(&outParam, data, len);
                    break;
                }
                case SQLITE_NULL: {
                    if constexpr (IsOptional<T>()) {
                        outParam = std::nullopt;
                    } else {
                        outParam = 0;
                    }
                    break;
                }
                default: {
                    // this should never happen :pray:
                    assert(false);
                    return Error("Unknown SQL type encountered, something isn't implemented yet");
                }
            }

            return std::nullopt;
        }
    };
};
