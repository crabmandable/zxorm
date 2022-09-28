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
        friend class Statement<Connection, Table...>;
        using statement_t = Statement<Connection, Table...>;
        using db_handle_ptr = std::unique_ptr<sqlite3, std::function<void(sqlite3*)>>;

        Connection(sqlite3* db_handle, Logger logger) {
            if (logger) _logger = logger;
            else _logger = [](auto...) {};

            _db_handle = {db_handle, [logger](sqlite3* handle) {
                int result = sqlite3_close_v2(handle);
                if (result != SQLITE_OK) {
                    const char* str = sqlite3_errstr(result);
                    if (logger) {
                        logger(LogLevel::Error, "Unable to destruct connection");
                        logger(LogLevel::Error, str);
                    }
                }
            }};
        }

        db_handle_ptr _db_handle;
        Logger _logger = nullptr;

        template<class C>
        struct TableForClass {
            static constexpr int idx = IndexOfFirst<std::is_same<C, typename Table::ObjectClass>::value...>::value;
            static_assert(idx >= 0, "Connection does not contain any table matching the type T");
            using type = typename std::tuple_element<idx, std::tuple<Table...>>::type;
        };
    public:
        static Result<Connection<Table...>> create(const char* fileName, int flags = 0, const char* zVfs = nullptr, Logger logger = nullptr) {
            if (!flags) {
                flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            }

            if (logger) {
                std::ostringstream oss;
                oss << "Opening sqlite connection with flags: " << flags;
                logger(LogLevel::Debug, oss.str().c_str());
            }

            sqlite3* db_handle = nullptr;
            int result = sqlite3_open_v2(fileName, &db_handle, flags, zVfs);
            if (result != SQLITE_OK || !db_handle) {
                const char* str = sqlite3_errstr(result);
                if (logger) {
                    logger(LogLevel::Error, "Unable to open sqlite connection");
                    logger(LogLevel::Error, str);
                }
                return Error("Unable to open sqlite connection", result);
            }

            return Connection({db_handle}, logger);
        }

        Connection(Connection&& old) = default;
        Connection& operator =(Connection&&) = default;
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;

        std::optional<Error> createTables(bool ifNotExist = true) {
            std::optional<Error> error = std::nullopt;
            // TODO make this a transaction

            std::array<statement_t,  sizeof...(Table)> statements = {statement_t(this, Table::createTableQuery(ifNotExist))...};

            for (auto& s : statements) {
                error = s.error;
                if (error) break;
                error = s.step();
                if (error) break;
            }

            return error;
        }

        template<class T>
        std::optional<Error> insertRecord(const T& record) {
            using table_t = typename TableForClass<T>::type;

            auto query = table_t::insertQuery();
            statement_t s = {this, query};
            if (s.error) {
                return s.error.value();
            }

            int i = 1;
            std::optional<Error> err;
            std::apply([&](const auto&... a) {
                ([&]() {
                    if (err) return;
                    using column_t = std::remove_reference_t<decltype(a)>;

                    if constexpr (!column_t::isAutoIncColumn) {
                        auto& val = column_t::getter(record);
                        err = s.bind(i++, val);
                    }

                }(), ...);
            }, typename table_t::columns_t{});

            if (err) {
                return err;
            }

            err = s.step();
            if (err) {
                return err;
            }

            if (!s.done) {
                return Error("Insert query didn't run to completion");
            }

            // TODO: fill the records primary key if needed

            return std::nullopt;
        }

        template<class T, typename PrimaryKeyType>
        OptionalResult<T> findRecord(const PrimaryKeyType& id)
        {
            using table_t = typename TableForClass<T>::type;

            static_assert(table_t::hasPrimaryKey, "Cannot execute a find on a table without a primary key");

            static_assert(std::is_convertible_v<PrimaryKeyType, typename table_t::PrimaryKey::MemberType>,
                    "Primary key type does not match the type specified in the definition of the table");

            typename table_t::PrimaryKey::MemberType pk = id;
            auto query = table_t::findQuery();
            statement_t s = {this, query};
            if (s.error) {
                return s.error.value();
            }
            std::optional<Error> err = s.bind(1, pk);
            if (err) {
                return err.value();
            }
            err = s.step();
            if (err) {
                return err.value();
            }

            if (s.done) {
                return std::nullopt;
            }

            if (s.columnCount != table_t::nColumns) {
                return Error("Unexpected number of columns returned by find query");
            }

            T record;
            size_t columnIdx = 0;
            std::apply([&](const auto&... a) {
                ([&]() {
                    if (err) return;
                    using column_t = std::remove_reference_t<decltype(a)>;
                    if constexpr (column_t::publicColumn) {
                        err = s.readColumn(columnIdx++, column_t::getter(record));
                    } else {
                        using value_t = typename column_t::MemberType;
                        value_t value;
                        err = s.readColumn(columnIdx++, value);
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
        std::optional<Error> deleteRecord(const PrimaryKeyType& id)
        {
            using table_t = typename TableForClass<T>::type;

            static_assert(table_t::hasPrimaryKey, "Cannot execute a delete on a table without a primary key");

            static_assert(std::is_convertible_v<PrimaryKeyType, typename table_t::PrimaryKey::MemberType>,
                    "Primary key type does not match the type specified in the definition of the table");

            typename table_t::PrimaryKey::MemberType pk = id;
            auto query = table_t::deleteQuery();
            statement_t s = {this, query};
            if (s.error) {
                return s.error.value();
            }
            std::optional<Error> err = s.bind(1, pk);
            if (err) {
                return err.value();
            }
            err = s.step();
            if (err) {
                return err.value();
            }

            return std::nullopt;
        }
    };
};
