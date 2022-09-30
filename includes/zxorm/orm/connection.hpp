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
    class Connection {
    static_assert(sizeof...(Table) > 0, "Connection should contain at least one table");

    private:
        using db_handle_ptr = std::unique_ptr<sqlite3, std::function<void(sqlite3*)>>;

        db_handle_ptr _db_handle;
        Logger _logger = nullptr;

        Connection(sqlite3* db_handle, Logger logger);

        template<class C> struct table_for_class;
        template<class C> static constexpr bool table_has_rowid();

        void log_error(const Error& err);
        inline void log_error(ZXORM_CONST_UNLESS_DEBUG OptionalError& err);

        auto make_statement(const std::string& query);
        auto exec(const std::string& query) -> OptionalError;

        // TODO this should be public
        OptionalError transaction(std::function<OptionalError()> run);

        // The conditional makes it impossible to deduce the type T apparently :(
        // so the public inserts call this implementation
        template<class T>
            OptionalError insert_record_impl (
                    std::conditional_t<table_has_rowid<T>(), T&, const T&> record);

    public:
        static Result<Connection<Table...>> create(
                const char* file_name, int flags = 0, const char* z_vfs = nullptr, Logger logger = nullptr);

        Connection(Connection&& old) = default;
        Connection& operator =(Connection&&) = default;
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;

        OptionalError create_tables(bool if_not_exist = true);
        Result<size_t> count_tables();

        template<class T>
            OptionalError insert_record (const T& record)
            { return insert_record_impl<T>(record); }

        // when the table has a rowid column
        // we set the id after insertion
        // therefore the record should not be const
        template<class T>
            requires (table_has_rowid<T>())
            OptionalError insert_record (T& record)
            { return insert_record_impl<T>(record); }

        template<class T, typename PrimaryKeyType>
            OptionalResult<T> find_record(const PrimaryKeyType& id);

        template<class T, typename PrimaryKeyType>
            OptionalError delete_record(const PrimaryKeyType& id);
    };

    template <class... Table>
    Result<Connection<Table...>> Connection<Table...>::create(
            const char* file_name,
            int flags,
            const char* z_vfs,
            Logger logger)
    {
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

    template <class... Table>
    Connection<Table...>::Connection(sqlite3* db_handle, Logger logger)
    {
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

    template <class... Table>
    template<class C>
    struct Connection<Table...>::table_for_class
    {
        static constexpr int idx = index_of_first<std::is_same<C, typename Table::object_class>::value...>::value;
        static_assert(idx >= 0, "Connection does not contain any table matching the type T");
        using type = typename std::tuple_element<idx, std::tuple<Table...>>::type;
    };

    template <class... Table>
    template<class C>
    constexpr bool Connection<Table...>::table_has_rowid()
    {
        using table_t = typename table_for_class<C>::type;
        if (!table_t::has_primary_key) return false;
        return table_t::primary_key_t::sql_column_type == sqlite_column_type::INTEGER;
    }

    template <class... Table>
    auto Connection<Table...>::make_statement(const std::string& query)
    {
        return Statement::create(_db_handle.get(), _logger, query);
    }

    template <class... Table>
    auto Connection<Table...>::exec(const std::string& query) -> OptionalError
    {
        auto stmt = make_statement(query);
        if (stmt.is_error()) return stmt.error();
        return stmt.value().step();
    }

    template <class... Table>
    void Connection<Table...>::log_error(const Error& err)
    {
        _logger(log_level::Error, std::string(err).c_str());
    }

    template <class... Table>
    inline void Connection<Table...>::log_error(ZXORM_CONST_UNLESS_DEBUG OptionalError& err)
    {
        if (err) {
            log_error(err.value());
        }
    }

    template <class... Table>
    OptionalError Connection<Table...>::transaction(std::function<OptionalError()> run)
    {
        auto err = exec("BEGIN TRANSACTION;");
        if (err) {
            _logger(log_level::Error, "Unable to begin transaction");
            log_error(err);
            return err;
        }

        err = run();
        if (err) {
            auto rollback_err = exec("ROLLBACK TRANSACTION;");
            if (rollback_err) {
                _logger(log_level::Error, "Unable to rollback transaction during error handling");
                log_error(rollback_err);
            }
            return err;
        }

        return exec("COMMIT TRANSACTION;");
    };

    template <class... Table>
    template<class T>
    OptionalError Connection<Table...>::insert_record_impl (
        std::conditional_t<table_has_rowid<T>(), T&, const T&> record)
    {
        using table_t = typename table_for_class<T>::type;

        auto query = table_t::insert_query();
        auto insert_stmt = make_statement(query);
        if (!insert_stmt) {
            return OptionalError(insert_stmt.error());
        }

        int i = 1;
        OptionalError err;
        std::apply([&](const auto&... a) {
            ([&]() {
                if (err) return;
                using column_t = std::remove_reference_t<decltype(a)>;

                if constexpr (!column_t::is_auto_inc_column) {
                    auto& val = column_t::getter(record);
                    err = insert_stmt.value().bind(i++, val);
                }

            }(), ...);
        }, typename table_t::columns_t{});

        if (err) {
            return err;
        }

        err = insert_stmt.value().step();
        if (err) {
            return err;
        }

        if (!insert_stmt.value().done()) {
            return Error("Insert query didn't run to completion");
        }

        if constexpr (table_has_rowid<T>()) {
            int64_t value = sqlite3_last_insert_rowid(_db_handle.get());
            table_t::primary_key_t::setter(record, value);
        }

        return std::nullopt;
    }

    template <class... Table>
    OptionalError Connection<Table...>::create_tables(bool if_not_exist)
    {

        std::array<Result<Statement>,  sizeof...(Table)> statements = {make_statement(Table::create_table_query(if_not_exist))...};

        return transaction([&]() -> OptionalError {
            for (auto& s : statements) {
                if (!s) {
                    return s.error();
                }
                auto error = s.value().step();
                if (error) return error;
            }

            return std::nullopt;
        });
    }

    template <class... Table>
    Result<size_t> Connection<Table...>::count_tables()
    {
        auto count_stmt = make_statement("SELECT COUNT(*) FROM `sqlite_schema` WHERE `type` = 'table';");
        if (count_stmt.is_error()) return count_stmt.error();

        auto err =  count_stmt.value().step();
        if (err) return err.value();

        ssize_t count;
        err = count_stmt.value().read_column(0, count);
        if (err) return err.value();
        return count;
    }

    template <class... Table>
    template<class T, typename PrimaryKeyType>
    OptionalResult<T> Connection<Table...>::find_record(const PrimaryKeyType& id)
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

        OptionalError err = stmt.bind(1, pk);
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

    template <class... Table>
    template<class T, typename PrimaryKeyType>
    OptionalError Connection<Table...>::delete_record(const PrimaryKeyType& id)
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
        OptionalError err = stmt.bind(1, pk);
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
