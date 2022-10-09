#pragma once

#include "zxorm/common.hpp"
#include "zxorm/error.hpp"
#include "zxorm/orm/types.hpp"
#include <cassert>
#include <cstring>
#include <iterator>
#include <optional>
#include <type_traits>
#include "zxorm/helpers/meta_container.hpp"

namespace zxorm {
    class Statement {
        private:
        Logger _logger;
        std::unique_ptr<sqlite3_stmt, std::function<void(sqlite3_stmt*)>> _stmt;
        size_t _parameter_count;
        std::map<size_t, bool> _is_bound;
        std::map<size_t, std::vector<uint8_t>> _blob_bindings;
        int _column_count = 0;
        size_t _step_count = 0;
        bool _done = false;

        Statement(Logger logger, sqlite3_stmt* stmt) : _logger{logger} {
            _stmt = {stmt, [logger](sqlite3_stmt* s) {
                int result = sqlite3_finalize(s);
                if (result != SQLITE_OK) {
                    const char* str = sqlite3_errstr(result);
                    logger(log_level::Error, "Unable to finalize statment");
                    logger(log_level::Error, str);
                }
            }};

            _parameter_count = sqlite3_bind_parameter_count(_stmt.get());
            for (size_t i = 1; i == _parameter_count; i++) {
                _is_bound[i] = false;
            }
        }

        public:
        [[nodiscard]] static Result<Statement> create(sqlite3* handle, Logger logger, const std::string& query)
        {
            logger(log_level::Debug, "Initializing statement");
            logger(log_level::Debug, query.c_str());

            sqlite3_stmt* stmt = nullptr;
            int result = sqlite3_prepare_v2(handle, query.c_str(), query.size() + 1, &stmt, nullptr);
            if (result != SQLITE_OK || !stmt) {
                const char* str = sqlite3_errstr(result);
                logger(log_level::Error, "Unable to initialize statement");
                logger(log_level::Error, str);
                return Error("Unable to initialize statement", result);
            }

            return Statement(logger, stmt);
        }

        [[nodiscard]] OptionalError bind(auto tuple)
        {
            OptionalError err;
            size_t i = 1;
            std::apply([&](const auto&... binding) {
                ([&]() {
                    if (err) return;
                    err = bind(i++, binding);
                }(), ...);
            }, tuple);

            return err;
        }

        template <ArithmeticT T>
        [[nodiscard]] OptionalError bind(size_t idx, const T& param)
        {
            // bindings start at 1 :(
            assert(idx != 0);
            using unwrapped_t = typename remove_optional<T>::type;
            const unwrapped_t* unwrapped;
            int result;
            bool bound_null = false;
            if constexpr (ignore_qualifiers::is_optional<T>()) {
                if (!param.has_value()) {
                    result = sqlite3_bind_null(_stmt.get(), idx);
                    bound_null = true;
                } else {
                    unwrapped = &param.value();
                }
            } else {
                unwrapped = &param;
            }

            if (!bound_null) {
                if constexpr (std::is_floating_point_v<unwrapped_t>) {
                    result = sqlite3_bind_double(_stmt.get(), idx, *unwrapped);
                } else {
                    if (sizeof(T) <= 4) {
                        result = sqlite3_bind_int(_stmt.get(), idx, *unwrapped);
                    } else {
                        result = sqlite3_bind_int64(_stmt.get(), idx, *unwrapped);
                    }
                }
            }

            if (result != SQLITE_OK) {
                return Error("Unable to bind parameter to statment", result);
            }
            _is_bound[idx] = true;
            return std::nullopt;
        }

        template <ContinuousContainer T>
        [[nodiscard]] OptionalError bind(size_t idx, const T& param)
        {
            assert(idx != 0);
            bool bound_null = false;
            int result;

            if constexpr (ignore_qualifiers::is_optional<T>()) {
                if (!param.has_value()) {
                    result = sqlite3_bind_null(_stmt.get(), idx);
                    bound_null = true;
                }
            }

            if (!bound_null) {
                constexpr size_t el_size = sizeof(typename remove_optional<T>::type::value_type);
                const auto param_to_bind = MetaContainer<const T>(param);
                auto len = param_to_bind.size() * el_size;
                _blob_bindings[idx] = std::vector<uint8_t>();
                _blob_bindings[idx].resize(len);
                std::memcpy(_blob_bindings[idx].data(), param_to_bind.data(), len);
                result = sqlite3_bind_blob(_stmt.get(), idx, _blob_bindings[idx].data(), _blob_bindings[idx].size(), nullptr);
            }

            if (result != SQLITE_OK) {
                return Error("Unable to bind parameter to statment", result);
            }

            _is_bound[idx] = true;

            return std::nullopt;
        }

        [[nodiscard]] OptionalError rewind() {
            int result = sqlite3_reset(_stmt.get());
            if (result != SQLITE_OK) {
                return Error( "Unable to reset statment", result);
            }
            _done = false;
            _step_count = 0;
            return std::nullopt;
        }

        [[nodiscard]] OptionalError reset() {
            auto err = rewind();
            if (err) return err;

            int result = sqlite3_clear_bindings(_stmt.get());
            if (result != SQLITE_OK) {
                return Error( "Unable to clear bindings", result);
            }
            for (auto& [_, v] : _is_bound) v = false;

            return std::nullopt;
        }

        [[nodiscard]] OptionalError step() {
            if (_done) {
                return Error("Query has run to completion");
            }
            if (std::any_of(_is_bound.cbegin(), _is_bound.cend(),
                        [](const auto& bound) { return !bound.second; } )) {
                return Error("Some parameters have not been bound");
            }

            int result = sqlite3_step(_stmt.get());
            _step_count++;
            if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
                return Error("Unable to execute statment", result);
            }

            _done = result == SQLITE_DONE;

            _column_count = sqlite3_column_count(_stmt.get());

            return std::nullopt;
        }

        template<ContinuousContainer T>
        [[nodiscard]] OptionalError read_column(size_t idx, T& out_param) {
            auto out = MetaContainer<T>(out_param);
            int data_type = sqlite3_column_type(_stmt.get(), idx);
            switch(data_type) {
                case SQLITE_INTEGER:
                case SQLITE_FLOAT: {
                    return Error("Tried to read an arithmetic column into a container");
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    const void* data = sqlite3_column_blob(_stmt.get(), idx);
                    size_t len = sqlite3_column_bytes(_stmt.get(), idx);
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
        [[nodiscard]] OptionalError read_column(size_t idx, T& out_param) {
            int data_type = sqlite3_column_type(_stmt.get(), idx);
            switch(data_type) {
                case SQLITE_INTEGER: {
                    if (sizeof(out_param) <= 4) {
                        out_param = sqlite3_column_int(_stmt.get(), idx);
                    } else {
                        out_param = sqlite3_column_int64(_stmt.get(), idx);
                    }
                    break;
                }
                case SQLITE_FLOAT: {
                    out_param = sqlite3_column_double(_stmt.get(), idx);
                    break;
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    return Error("Tried to read an arithmetic column into a container");
                    size_t len = sqlite3_column_bytes(_stmt.get(), idx);
                    if (len > sizeof(T)) {
                        return Error("Param does not have enough space to fit column contents");
                    }
                    const void* data = sqlite3_column_blob(_stmt.get(), idx);
                    std::memcpy(&out_param, data, len);
                    break;
                }
                case SQLITE_NULL: {
                    if constexpr (ignore_qualifiers::is_optional<T>()) {
                        out_param = std::nullopt;
                    } else {
                        out_param = 0;
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

        int column_count() { return _column_count; }
        bool done() { return _done; }
        bool step_count() { return _step_count; }
    };
};
