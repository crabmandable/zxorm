#pragma once
#include <cassert>
#include <cstring>
#include <iterator>
#include <optional>
#include <type_traits>
#include <memory>
#include <map>

#include "zxorm/common.hpp"
#include "zxorm/error.hpp"
#include "zxorm/orm/types.hpp"
#include "zxorm/orm/field.hpp"
#include "zxorm/helpers/meta_container.hpp"

namespace zxorm {
    class Statement {
        private:
        sqlite3* _handle;
        Logger _logger;
        std::unique_ptr<sqlite3_stmt, std::function<void(sqlite3_stmt*)>> _stmt;
        size_t _parameter_count;
        std::map<size_t, bool> _is_bound;
        std::map<size_t, std::vector<uint8_t>> _blob_bindings;
        int _column_count = 0;
        size_t _step_count = 0;
        bool _done = false;

        public:
        Statement(sqlite3* handle, Logger logger, const std::string& query) : _handle{handle}, _logger{logger}
        {
            _logger(log_level::Debug, "Initializing statement");
            _logger(log_level::Debug, query.c_str());

            sqlite3_stmt* stmt = nullptr;
            int result = sqlite3_prepare_v2(handle, query.c_str(), query.size() + 1, &stmt, nullptr);
            if (result != SQLITE_OK || !stmt) {
                auto err = SQLExecutionError("Unable to initialize statement", handle);
                _logger(log_level::Error, err);
                throw err;
            }

            _stmt = {stmt, [logger, handle](sqlite3_stmt* s) {
                int result = sqlite3_finalize(s);
                if (result != SQLITE_OK) {
                    auto err = InternalError("Statement error", handle);
                    logger(log_level::Error, err);
                }
            }};

            _parameter_count = sqlite3_bind_parameter_count(_stmt.get());
            for (size_t i = 1; i == _parameter_count; i++) {
                _is_bound[i] = false;
            }
        }

        void bind(auto tuple)
        {
            size_t i = 1;
            std::apply([&](const auto&... binding) {
                ([&]() {
                    if constexpr (is_multi_value_binding<std::remove_cvref_t<decltype(binding)>>::value) {
                        for (size_t j = 0; j < binding.size(); j++) {
                            bind(i++, binding[j]);
                        }
                    } else {
                        bind(i++, binding);
                    }
                }(), ...);
            }, tuple);
        }

        template <ArithmeticT T>
        void bind(size_t idx, const T& param)
        {
            // bindings start at 1 :(
            assert(idx != 0);
            using unwrapped_t = typename remove_optional<T>::type;
            const unwrapped_t* unwrapped;
            int result = SQLITE_OK;
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
                throw InternalError("Unable to bind parameter to statment", _handle);
            }
            _is_bound[idx] = true;
        }

        template <ContinuousContainer T>
        void bind(size_t idx, const T& param)
        {
            assert(idx != 0);
            bool bound_null = false;
            int result = SQLITE_OK;

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
                throw InternalError("Unable to bind parameter to statment", _handle);
            }

            _is_bound[idx] = true;
        }

        void rewind() {
            int result = sqlite3_reset(_stmt.get());
            if (result != SQLITE_OK) {
                throw InternalError( "Unable to reset statment", _handle);
            }
            _done = false;
            _step_count = 0;
        }

        void reset() {
            rewind();

            int result = sqlite3_clear_bindings(_stmt.get());
            if (result != SQLITE_OK) {
                throw InternalError( "Unable to clear bindings", _handle);
            }
            for (auto& [_, v] : _is_bound) v = false;
        }

        void step() {
            if (_done) {
                throw InternalError("Query has run to completion");
            }
            if (std::any_of(_is_bound.cbegin(), _is_bound.cend(),
                        [](const auto& bound) { return !bound.second; } )) {
                throw InternalError("Some parameters have not been bound");
            }

            int result = sqlite3_step(_stmt.get());
            _step_count++;
            if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
                if (is_constraint_error(result)) {
                    throw SQLConstraintError("Constraint failed", _handle);
                } else {
                    throw SQLExecutionError("Unable to execute statment", _handle);
                }
            }

            _done = result == SQLITE_DONE;

            _column_count = sqlite3_column_count(_stmt.get());
        }

        template<ContinuousContainer T>
        void read_column(size_t idx, T& out_param) {
            auto out = MetaContainer<T>(out_param);
            int data_type = sqlite3_column_type(_stmt.get(), idx);
            switch(data_type) {
                case SQLITE_INTEGER:
                case SQLITE_FLOAT: {
                    throw InternalError("Tried to read an arithmetic column into a container");
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    const void* data = sqlite3_column_blob(_stmt.get(), idx);
                    size_t len = sqlite3_column_bytes(_stmt.get(), idx);
                    if (!out.resize(len)) {
                        throw InternalError("Container does not have enough space to read column data");
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
                    throw InternalError("Unknown SQL type encountered, something isn't implemented yet");
                }
            }
        }

        template<ArithmeticT T>
        void read_column(size_t idx, T& out_param) {
            int data_type = sqlite3_column_type(_stmt.get(), idx);
            switch(data_type) {
                case SQLITE_INTEGER: {
                    if (sizeof(out_param) <= 4) {
                        out_param = static_cast<T>(sqlite3_column_int(_stmt.get(), idx));
                    } else {
                        out_param = static_cast<T>(sqlite3_column_int64(_stmt.get(), idx));
                    }
                    break;
                }
                case SQLITE_FLOAT: {
                    out_param = static_cast<T>(sqlite3_column_double(_stmt.get(), idx));
                    break;
                }
                case SQLITE_TEXT:
                case SQLITE_BLOB: {
                    throw InternalError("Tried to read an arithmetic column into a container");
                    size_t len = sqlite3_column_bytes(_stmt.get(), idx);
                    if (len > sizeof(T)) {
                        throw InternalError("Param does not have enough space to fit column contents");
                    }
                    const void* data = sqlite3_column_blob(_stmt.get(), idx);
                    std::memcpy(&out_param, data, len);
                    break;
                }
                case SQLITE_NULL: {
                    if constexpr (ignore_qualifiers::is_optional<T>()) {
                        out_param = std::nullopt;
                    } else {
                        out_param = static_cast<T>(0);
                    }
                    break;
                }
                default: {
                    // this should never happen :pray:
                    assert(false);
                    throw InternalError("Unknown SQL type encountered, something isn't implemented yet");
                }
            }
        }

        int column_count() { return _column_count; }
        bool done() { return _done; }
        bool step_count() { return _step_count; }
    };
};
