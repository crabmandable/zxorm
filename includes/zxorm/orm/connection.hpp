#pragma once

#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/error.hpp"
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
        inline void log_error(const OptionalError& err);

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
    inline void Connection<Table...>::log_error(const OptionalError& err)
    {
        if (err) {
            log_error(err.value());
        }
    }

    template <class... Table>
    OptionalError Connection<Table...>::transaction(std::function<OptionalError()> run)
    {
        ZXORM_TRY(exec("BEGIN TRANSACTION;"));

        auto err = run();
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
        ZXORM_GET_RESULT(Statement insert_stmt, make_statement(query));

        int i = 1;
        OptionalError err;
        std::apply([&](const auto&... a) {
            ([&]() {
                if (err) return;
                using column_t = std::remove_reference_t<decltype(a)>;

                if constexpr (!column_t::is_auto_inc_column) {
                    auto& val = column_t::getter(record);
                    err = insert_stmt.bind(i++, val);
                }

            }(), ...);
        }, typename table_t::columns_t{});

        if (err) {
            return err;
        }

        ZXORM_TRY(insert_stmt.step());

        if (!insert_stmt.done()) {
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

        std::array<Result<Statement>,  sizeof...(Table)> statements =
            { make_statement(Table::create_table_query(if_not_exist))... };

        return transaction([&]() -> OptionalError {
            for (auto& s : statements) {
                if (!s) {
                    return s.error();
                }
                ZXORM_TRY(s.value().step());
            }

            return std::nullopt;
        });
    }

    template <class... Table>
    Result<size_t> Connection<Table...>::count_tables()
    {
        ZXORM_GET_RESULT(
                auto count_stmt,
                make_statement("SELECT COUNT(*) FROM `sqlite_schema` WHERE `type` = 'table';"));

        ZXORM_TRY(count_stmt.step());

        ssize_t count;
        ZXORM_TRY(count_stmt.read_column(0, count));
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

        ZXORM_GET_RESULT(Statement stmt, make_statement(query));
        ZXORM_TRY(stmt.bind(1, pk));
        ZXORM_TRY(stmt.step());

        if (stmt.done()) {
            return std::nullopt;
        }

        if (stmt.column_count() != table_t::n_columns) {
            return Error("Unexpected number of columns returned by find query,"
                    " tables may not be synced");
        }

        T record;
        size_t column_idx = 0;
        OptionalError err;
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

        ZXORM_GET_RESULT(Statement stmt, make_statement(query));
        ZXORM_TRY(stmt.bind(1, pk));
        ZXORM_TRY(stmt.step());

        return std::nullopt;
    }
};
