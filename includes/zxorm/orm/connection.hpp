#pragma once

#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/result.hpp"
#include <functional>
#include <sstream>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace zxorm {
    template <class... Table>
    requires(sizeof...(Table) > 0)
    class Connection {
    private:
        using db_handle_ptr = std::unique_ptr<sqlite3, std::function<void(sqlite3*)>>;

        Connection(sqlite3* db_handle, Logger logger) {
            if (logger) _logger = logger;
            else _logger = [](auto...) {};

            _db_handle = {db_handle, [logger](sqlite3* handle) {
                int result = sqlite3_close_v2(handle);
                if (result != SQLITE_OK) {
                    const char* str = sqlite3_errstr(result);
                    if (logger) {
                        logger(log_level::Error, "Unable to destruct connection");
                        logger(log_level::Error, str);
                    }
                }
            }};
        }

        db_handle_ptr _db_handle;
        Logger _logger = nullptr;

        template<class C>
        struct table_for_class {
            static constexpr int idx = index_of_first<std::is_same<C, typename Table::object_class>::value...>::value;
            static_assert(idx >= 0, "Connection does not contain any table matching the type T");
            using type = typename std::tuple_element<idx, std::tuple<Table...>>::type;
        };

        auto make_statement(const std::string& query) {
            return Statement::create(_db_handle.get(), _logger, query);
        }

    public:
        static Result<Connection<Table...>> create(const char* file_name, int flags = 0, const char* z_vfs = nullptr, Logger logger = nullptr) {
            if (!flags) {
                flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            }

            if (logger) {
                std::ostringstream oss;
                oss << "Opening sqlite connection with flags: " << flags;
                logger(log_level::Debug, oss.str().c_str());
            }

            sqlite3* db_handle = nullptr;
            int result = sqlite3_open_v2(file_name, &db_handle, flags, z_vfs);
            if (result != SQLITE_OK || !db_handle) {
                const char* str = sqlite3_errstr(result);
                if (logger) {
                    logger(log_level::Error, "Unable to open sqlite connection");
                    logger(log_level::Error, str);
                }
                return Error("Unable to open sqlite connection", result);
            }

            return Connection({db_handle}, logger);
        }

        Connection(Connection&& old) = default;
        Connection& operator =(Connection&&) = default;
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;

        std::optional<Error> create_tables(bool if_not_exist = true) {
            // TODO make this a transaction

            std::array<Result<Statement>,  sizeof...(Table)> statements = {make_statement(Table::create_table_query(if_not_exist))...};

            for (auto& s : statements) {
                if (!s) {
                    return s.error();
                }
                auto error = s.value().step();
                if (error) return error;
            }

            return std::nullopt;
        }

        template<class T>
        std::optional<Error> insert_record(const T& record) {
            using table_t = typename table_for_class<T>::type;

            auto query = table_t::insert_query();
            auto s = make_statement(query);
            if (!s) {
                return s.error();
            }

            Statement stmt = s.value();

            int i = 1;
            std::optional<Error> err;
            std::apply([&](const auto&... a) {
                ([&]() {
                    if (err) return;
                    using column_t = std::remove_reference_t<decltype(a)>;

                    if constexpr (!column_t::is_auto_inc_column) {
                        auto& val = column_t::getter(record);
                        err = stmt.bind(i++, val);
                    }

                }(), ...);
            }, typename table_t::columns_t{});

            if (err) {
                return err;
            }

            err = stmt.step();
            if (err) {
                return err;
            }

            if (!stmt.done()) {
                return Error("Insert query didn't run to completion");
            }

            // TODO: fill the records primary key if needed

            return std::nullopt;
        }

        template<class T, typename PrimaryKeyType>
        OptionalResult<T> find_record(const PrimaryKeyType& id)
        {
            using table_t = typename table_for_class<T>::type;

            static_assert(table_t::has_primary_key, "Cannot execute a find on a table without a primary key");

            static_assert(std::is_convertible_v<PrimaryKeyType, typename table_t::primary_key_t::member_t>,
                    "Primary key type does not match the type specified in the definition of the table");

            typename table_t::primary_key_t::member_t pk = id;
            auto query = table_t::find_query();
            auto s = make_statement(query);
            if (s.is_error()) {
                return s.error();
            }
            Statement stmt = s;

            std::optional<Error> err = stmt.bind(1, pk);
            if (err) {
                return err.value();
            }
            err = stmt.step();
            if (err) {
                return err.value();
            }

            if (stmt.done()) {
                return std::nullopt;
            }

            if (stmt.column_count() != table_t::n_columns) {
                return Error("Unexpected number of columns returned by find query");
            }

            T record;
            size_t column_idx = 0;
            std::apply([&](const auto&... a) {
                ([&]() {
                    if (err) return;
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (column_t::public_column) {
                        err = stmt.read_column(column_idx++, column_t::getter(record));
                    } else {
                        using value_t = typename column_t::member_t;
                        value_t value;
                        err = stmt.read_column(column_idx++, value);
                        column_t::setter(record, value);
                    }
                }(), ...);
            }, typename table_t::columns_t{});

            if (err) {
                return err.value();
            }

            return record;
        }

        template<class T, typename PrimaryKeyType>
        std::optional<Error> delete_record(const PrimaryKeyType& id)
        {
            using table_t = typename table_for_class<T>::type;

            static_assert(table_t::has_primary_key, "Cannot execute a delete on a table without a primary key");

            static_assert(std::is_convertible_v<PrimaryKeyType, typename table_t::primary_key_t::member_t>,
                    "Primary key type does not match the type specified in the definition of the table");

            typename table_t::primary_key_t::member_t pk = id;
            auto query = table_t::delete_query();
            auto s = make_statement(query);
            if (!s) {
                return s.error();
            }
            Statement stmt = s;
            std::optional<Error> err = stmt.bind(1, pk);
            if (err) {
                return err.value();
            }
            err = stmt.step();
            if (err) {
                return err.value();
            }

            return std::nullopt;
        }
    };
};
