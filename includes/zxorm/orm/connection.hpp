#pragma once

#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/error.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/expression.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/field.hpp"
#include "zxorm/orm/query/select_query.hpp"
#include "zxorm/orm/query/delete_query.hpp"
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

        template<class C> struct index_of_table;
        template<FixedLengthString name> struct index_of_table_name;

        template<class C> static constexpr bool table_has_rowid();

        template<bool results_are_optional, typename selectables_tuple, typename T>
        struct select_type;

        template <typename T>
        struct select_or_table_table;

        template<typename T, typename OtherTablesTuple>
        struct join_clause_type;

        template <typename JoinedTables, typename JoinedClauses, typename Last, typename... T>
        struct make_joins;

        void log_error(const Error& err);
        inline void log_error(const OptionalError& err);

        template<class SelectOrTable, class... Joins>
        auto make_select_query();

        template<class From>
        auto make_delete_query();

        auto make_statement(const std::string& query);
        auto exec(const std::string& query) -> OptionalError;

        // The conditional makes it impossible to deduce the type T apparently :(
        // so the public inserts call this implementation
        template<class T>
            [[nodiscard]] OptionalError insert_record_impl (
                    std::conditional_t<table_has_rowid<T>(), T&, const T&> record);

    public:

        template<class C> struct table_for_class;

        template <class C>
        using table_for_class_t = typename table_for_class<C>::type;

        template<FixedLengthString name> struct table_for_name;

        template <FixedLengthString name>
        using table_for_name_t = typename table_for_name<name>::type;


        static Result<Connection<Table...>> create(
                const char* file_name, int flags = 0, const char* z_vfs = nullptr, Logger logger = nullptr);

        Connection(Connection&& old) = default;
        Connection& operator =(Connection&&) = default;
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;

        OptionalError create_tables(bool if_not_exist = true);
        Result<size_t> count_tables();

        // TODO needs tests
        OptionalError transaction(std::function<OptionalError()> run);

        template<class T>
            OptionalError update_record (const T& record);

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

        // matches stl containers, allowing template arg deduction
        template<template<typename, typename> typename Container, typename T, typename Allocator>
            OptionalError insert_many_records (const Container<T, Allocator>& records, size_t batch_size = 10)
            { return insert_many_records<T>(records, batch_size); }

        // matches std::array, allowing template arg deduction
        template<template<typename, auto> typename Container, typename T, auto size>
            OptionalError insert_many_records (const Container<T, size>& records, size_t batch_size = 10)
            { return insert_many_records<T>(records, batch_size); }

        template<typename T, IndexableContainer<T> Container>
            OptionalError insert_many_records (const Container& records, size_t batch_size = 10);

        template<class T, typename PrimaryKeyType>
            [[nodiscard]] OptionalResult<T> find_record(const PrimaryKeyType& id);

        template<class T, typename PrimaryKeyType>
            OptionalError delete_record(const PrimaryKeyType& id);

        template<class Select, class... Joins>
            [[nodiscard]] auto select_query();

        template<class From>
            [[nodiscard]] auto delete_query();


        template<class T>
        [[nodiscard]] auto first();

        template<class T>
        [[nodiscard]] auto last();

        template<class T>
        OptionalError truncate();
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
            const char* str = db_handle ? sqlite3_errmsg(db_handle) : sqlite3_errstr(result);
            if (logger) {
                logger(log_level::Error, "Unable to open sqlite connection");
                logger(log_level::Error, str);
            }
            return Error("Unable to open sqlite connection", db_handle);
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
                const char* str = sqlite3_errmsg(handle);
                if (logger) {
                    logger(log_level::Error, "Unable to destruct connection");
                    logger(log_level::Error, str);
                }
            }
        }};
    }

    template <class... Table>
    template<class C>
    struct Connection<Table...>::index_of_table
    {
        static constexpr int value = index_of_first<
            (std::is_same_v<C, typename Table::object_class> || std::is_same_v<C, Table>)...
            >::value;
    };

    template <class... Table>
    template<FixedLengthString name>
    struct Connection<Table...>::index_of_table_name
    {
        static constexpr int value = index_of_first<(name == Table::name)...>::value;
    };

    template <class... Table>
    template<class C>
    struct Connection<Table...>::table_for_class
    {
        static_assert(index_of_table<C>::value >= 0, "Connection does not contain any table matching the type T");
        using type = typename std::tuple_element<index_of_table<C>::value, std::tuple<Table...>>::type;
    };

    template <class... Table>
    template<FixedLengthString name>
    struct Connection<Table...>::table_for_name
    {
        static constexpr int idx = index_of_table_name<name>::value;
        static_assert(idx >= 0, "Connection does not contain any table matching this name");
        using type = typename std::tuple_element<idx, std::tuple<Table...>>::type;
    };

    template <class... Table>
    template<class C>
    constexpr bool Connection<Table...>::table_has_rowid()
    {
        using table_t = table_for_class_t<C>;
        if (!table_t::has_primary_key) return false;
        return table_t::primary_key_t::sql_column_type == sqlite_column_type::INTEGER;
    }


    template <FixedLengthString table_name, typename selectables_tuple>
    struct selection_exists: std::false_type {};

    template <FixedLengthString table_name, typename... T>
    struct selection_exists<table_name, std::tuple<T...>> : std::bool_constant<(... || (T::table_name == table_name))> {};

    template <FixedLengthString table_name, typename selectables_tuple>
    static constexpr bool selection_exists_v = selection_exists<table_name, selectables_tuple>::value;

    template <FixedLengthString table_name, typename selectables_tuple>
    struct selection_is_optional: std::false_type {};

    template <FixedLengthString table_name, typename... T>
    struct selection_is_optional<table_name, std::tuple<T...>>: std::bool_constant<
        not (... && (table_name != T::table_name || not T::is_optional))
    > {};

    template <FixedLengthString table_name, typename selectables_tuple>
    static constexpr bool selection_is_optional_v = selection_is_optional<table_name, selectables_tuple>::value;

    template<typename selectables_tuple, typename table>
    struct selectable_for_clause : std::type_identity<
        __selectable<selection_is_optional_v<table::name, selectables_tuple>, table>
    >{};

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename T>
    struct Connection<Table...>::select_type : std::type_identity<__select_impl<false, __selectable<false, table_for_class_t<T>>>> { };

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename... U>
    struct Connection<Table...>::select_type<results_are_optional, selectables_tuple, Select<From, U...>> {
        using type = __select_impl<
            results_are_optional,
            typename selectable_for_clause<selectables_tuple, table_for_class_t<From>>::type,
            typename selectable_for_clause<selectables_tuple, table_for_class_t<U>>::type...
        >;
    };


    template <class... Table>
    template<typename T, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type : std::false_type {};

    template <class... Table>
    template <typename T, join_type_t type, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type<Join<T, type>, OtherTablesTuple> : std::type_identity<
        __join_impl<table_for_class_t<T>::name, type, OtherTablesTuple>
    >{};

    template <class... Table>
    template<Field field_a, Field field_b, join_type_t type, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type<JoinOn<field_a, field_b, type>, OtherTablesTuple> : std::type_identity<
        __join_on_impl<field_a, field_b, type, OtherTablesTuple>
    >{};


    template <class... Table>
    template <typename JoinedTables, typename JoinedClauses, typename Last, typename... T>
    struct Connection<Table...>::make_joins {
        using join_t = typename join_clause_type<Last, JoinedTables>::type;
        using type = decltype(std::tuple_cat(JoinedClauses{}, std::tuple<join_t>{}));
        static constexpr bool query_is_nested = join_t::is_nested;
    };

    template <class... Table>
    template <typename JoinedTables, typename JoinedClauses, typename Current, typename Next, typename... Tails>
    struct Connection<Table...>::make_joins<JoinedTables, JoinedClauses, Current, Next, Tails...> {
        using join_t = typename join_clause_type<Current, JoinedTables>::type;
        using joined_clauses_t = decltype(std::tuple_cat(JoinedClauses{}, std::tuple<join_t>{}));
        using joined_tables_t = decltype(std::tuple_cat(JoinedTables{}, std::tuple<table_for_name_t<join_t::right_table_name>>{}));
        using type = typename make_joins<joined_tables_t, joined_clauses_t, Next, Tails...>::type;
        static constexpr bool query_is_nested = join_t::is_nested ||  make_joins<joined_tables_t, joined_clauses_t, Next, Tails...>::query_is_nested;
    };

    template <class... Table>
    template <typename T>
    struct Connection<Table...>::select_or_table_table : std::type_identity<table_for_class_t<T>> {};

    template <class... Table>
    template <typename From, typename... T>
    struct Connection<Table...>::select_or_table_table<Select<From, T...>> : std::type_identity<table_for_class_t<From>> {};

    template<class Join>
    auto get_seletables_from_join(Join j) {
        return typename decltype(j)::selectables_tuple {};
    }

    template <typename T>
    struct selectables_from_joins;

    template <typename... T>
    struct selectables_from_joins<std::tuple<T...>> {
        using nested = std::tuple<decltype(get_seletables_from_join(std::declval<T>()))...>;
        using type = decltype(flatten_tuple(std::declval<nested>()));
    };

    template <typename T>
    struct a_selection_is_optional;

    template <typename... T>
    struct a_selection_is_optional<std::tuple<T...>> : std::bool_constant<(... || T::is_optional)> {};

    template <class... Table>
    template<typename SelectOrTable, typename... Joins>
    auto Connection<Table...>::make_select_query()
    {
        static_assert(index_of_table<SelectOrTable>::value >= 0 || is_select_v<SelectOrTable>,
                "Template argument for select queries should be a table to select, "
                "or an `Select` template containing multiple tables");

        if constexpr (sizeof...(Joins)) {
            static_assert(all_of<is_join<Joins>...>,
                    "Template arguments for select query should be a `Join` or `JoinOn` clause");

            using join_meta = make_joins<
                std::tuple<typename select_or_table_table<SelectOrTable>::type>,
                std::tuple<>,
                Joins...>;

            using joins_tuple = typename join_meta::type;
            static constexpr bool query_is_nested = join_meta::query_is_nested;

            using selectables_t = typename selectables_from_joins<joins_tuple>::type;

            static constexpr bool results_are_optional = query_is_nested && a_selection_is_optional<selectables_t>::value;
            return std::apply([&](const auto&... a) {
                return SelectQuery<typename select_type<results_are_optional, selectables_t, SelectOrTable>::type, std::remove_reference_t<decltype(a)>...> (
                    _db_handle.get(),
                    _logger
                );
            }, joins_tuple{});
        } else {
            return SelectQuery<typename select_type<false, std::tuple<>, SelectOrTable>::type> (
                _db_handle.get(),
                _logger
            );
        }
    }

    template <class... Table>
    template<class From>
    auto Connection<Table...>::make_delete_query()
    {
        return DeleteQuery<table_for_class_t<From>>(
            _db_handle.get(),
            _logger
        );
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
        _logger(log_level::Error, err);
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
    OptionalError Connection<Table...>::update_record (const T& record)
    {
        using table_t = table_for_class_t<T>;
        static_assert(table_t::has_primary_key, "Cannot execute an update on a table without a primary key");

        auto& pk = table_t::primary_key_t::getter(record);

        if constexpr (table_has_rowid<T>()) {
            if (!pk) {
                return Error("Cannot update record with unknown rowid");
            }
        }

        auto query = table_t::update_query();
        ZXORM_GET_RESULT(Statement update_stmt, make_statement(query));

        int i = 1;
        OptionalError err;
        std::apply([&](const auto&... a) {
            ([&]() {
                if (err) return;
                using column_t = std::remove_reference_t<decltype(a)>;

                if constexpr (not column_t::is_primary_key) {
                    auto& val = column_t::getter(record);
                    err = update_stmt.bind(i++, val);
                }

            }(), ...);
        }, typename table_t::columns_t{});

        if (err) {
            return err;
        }

        ZXORM_TRY(update_stmt.bind(i++, pk));
        ZXORM_TRY(update_stmt.step());

        if (!update_stmt.done()) {
            return Error("Update query didn't run to completion");
        }

        return std::nullopt;
    }

    template <class... Table>
    template<typename T, IndexableContainer<T> Container>
    OptionalError Connection<Table...>::insert_many_records (const Container& records, size_t batch_size)
    {
        using table_t = table_for_class_t<T>;
        return transaction([&]() -> OptionalError {
            size_t inserted = 0;
            // this error should always be overwritten
            Result<Statement> insert_stmt = Error("Uninitialized statement");

            while (inserted < records.size()) {
                // iteration is less than batch size, query needs to be [re-]initialized
                if (records.size() - inserted < batch_size) {
                    batch_size = records.size() - inserted;
                    auto query = table_t::insert_query(batch_size);
                    insert_stmt = make_statement(query);
                    if (insert_stmt.is_error()) {
                        return insert_stmt.error();
                    }
                }
                // first itertion, initiaize statment
                else if (inserted == 0) {
                    auto query = table_t::insert_query(batch_size);
                    insert_stmt = make_statement(query);
                    if (insert_stmt.is_error()) {
                        return insert_stmt.error();
                    }
                // subsequent iterations just reset the same query
                } else {
                    ZXORM_TRY(insert_stmt.value().reset());
                }


                int i = 1;
                for (size_t row = 0; row < batch_size; row++) {
                    OptionalError err;
                    std::apply([&](const auto&... a) {
                        ([&]() {
                            if (err) return;
                            using column_t = std::remove_reference_t<decltype(a)>;

                            if constexpr (!column_t::is_auto_inc_column) {
                                const auto& val = column_t::getter(records[row + inserted]);
                                err = insert_stmt.value().bind(i++, val);
                            }

                        }(), ...);
                    }, typename table_t::columns_t{});

                    if (err) {
                        return err;
                    }
                }

                ZXORM_TRY(insert_stmt.value().step());

                if (!insert_stmt.value().done()) {
                    return Error("Insert query didn't run to completion");
                }

                inserted += batch_size;
            }

            return std::nullopt;
        });
    }

    template <class... Table>
    template<class T>
    OptionalError Connection<Table...>::insert_record_impl (
        std::conditional_t<table_has_rowid<T>(), T&, const T&> record)
    {
        using table_t = table_for_class_t<T>;

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
        using table_t = table_for_class_t<T>;

        static_assert(table_t::has_primary_key, "Cannot execute a find on a table without a primary key");

        using primary_key_t = typename table_t::primary_key_t;

        static_assert(std::is_convertible_v<PrimaryKeyType, typename primary_key_t::member_t>,
                "Primary key type does not match the type specified in the definition of the table");

        auto e = Field<table_t, primary_key_t::name>() == id;
        return make_select_query<T>().where(e).one();
    }

    template <class... Table>
    template<class T, typename PrimaryKeyType>
    OptionalError Connection<Table...>::delete_record(const PrimaryKeyType& id)
    {
        using table_t = table_for_class_t<T>;

        static_assert(table_t::has_primary_key, "Cannot execute a delete on a table without a primary key");

        using primary_key_t = typename table_t::primary_key_t;

        static_assert(std::is_convertible_v<PrimaryKeyType, typename primary_key_t::member_t>,
                "Primary key type does not match the type specified in the definition of the table");

        return make_delete_query<T>().where(Field<table_t, primary_key_t::name>() == id).exec();
    }

    template <class... Table>
    template<class From>
    auto Connection<Table...>::delete_query()
    {
        return make_delete_query<From>();
    }

    template <class... Table>
    template<typename Select, typename... Joins>
    auto Connection<Table...>::select_query()
    {
        return make_select_query<Select, Joins...>();
    }

    template <class... Table>
    template<class T>
    auto Connection<Table...>::first()
    {
        return make_select_query<T>().one();
    }

    template <class... Table>
    template<class T>
    auto Connection<Table...>::last()
    {
        using table_t = table_for_class_t<T>;
        constexpr auto pk_name = table_t::primary_key_t::name;
        return make_select_query<T>().template order_by<pk_name>(order_t::DESC).one();
    }

    template <class... Table>
    template<class T>
    OptionalError Connection<Table...>::truncate()
    {
        using table_t = table_for_class_t<T>;

        ZXORM_GET_RESULT(
            auto stmt,
            make_statement(std::string("DELETE FROM `") + table_t::name.value + "`;")
        );

        return stmt.step();
    }
};
