#pragma once

#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/error.hpp"
#include "zxorm/orm/statement.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/expression.hpp"
#include "zxorm/orm/field.hpp"
#include "zxorm/orm/query/select_query.hpp"
#include "zxorm/orm/query/delete_query.hpp"
#include <functional>
#include <sstream>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace zxorm {
    template <class... Table>
    class Connection {
    static_assert(sizeof...(Table) > 0, "Connection should contain at least one table");

    private:
        using db_handle_ptr = std::unique_ptr<sqlite3, std::function<void(sqlite3*)>>;

        db_handle_ptr _db_handle;
        Logger _logger = nullptr;

        // more complicated queries are omitted, since this is only relevant for things that can be cached
        // i.e. queries that always have the same number of binds
        enum class QueryCacheType {
            find_record,
            delete_record,
            first,
            last,
        };

        std::unordered_map<
            std::string_view, // table name
            std::unordered_map<
                QueryCacheType,
                std::shared_ptr<BasePreparedQuery>
            >
        > _query_cache;

        // theset don't use the query template
        std::unordered_map<std::string_view /* table name */, std::shared_ptr<Statement>> _insert_statement_cache;
        std::unordered_map<std::string_view /* table name */, std::shared_ptr<Statement>> _update_statement_cache;

        Connection(sqlite3* db_handle, Logger logger);

        template<class C> struct index_of_table;
        template<FixedLengthString name> struct index_of_table_name;

        template<class C> static constexpr bool table_has_rowid();

        template<bool results_are_optional, typename selectables_tuple, typename From, typename T>
        struct select_type;

        template<bool results_are_optional, typename selectables_tuple, typename From, typename T>
        struct selection_for_clause;

        template <typename T>
        struct select_or_table_table;

        template<typename T, typename OtherTablesTuple>
        struct join_clause_type;

        template <typename JoinedTables, typename JoinedClauses, typename Last=void, typename... T>
        struct make_joins;

        template <typename selectables_tuple, typename T>
        struct selections_are_selectable;

        void log_error(const Error& err);

        template<class SelectOrTable, typename From=void, class... Clauses>
        auto make_select_query();

        template<class From>
        auto make_delete_query();

        auto make_statement(const std::string& query);
        void exec(const std::string& query);

        // The conditional makes it impossible to deduce the type T apparently :(
        // so the public inserts call this implementation
        template<class T>
            void insert_record_impl (
                    std::conditional_t<table_has_rowid<T>(), T&, const T&> record);

    public:

        template<class C> struct table_for_class;

        template <class C>
        using table_for_class_t = typename table_for_class<C>::type;

        template<FixedLengthString name> struct table_for_name;

        template <FixedLengthString name>
        using table_for_name_t = typename table_for_name<name>::type;


        static Connection<Table...> create(
                const char* file_name, int flags = 0, const char* z_vfs = nullptr, Logger logger = nullptr);

        Connection(Connection&& old) = default;
        Connection& operator =(Connection&&) = default;
        Connection(const Connection&) = delete;
        Connection operator =(const Connection&) = delete;

        void create_tables(bool if_not_exist = true);
        size_t count_tables();

        void transaction(std::function<void()> run);

        template<class T>
            void update_record (const T& record);

        template<class T>
            void insert_record (const T& record)
            { return insert_record_impl<T>(record); }

        // when the table has a rowid column
        // we set the id after insertion
        // therefore the record should not be const
        template<class T>
            requires (table_has_rowid<T>())
            void insert_record (T& record)
            { return insert_record_impl<T>(record); }

        // matches stl containers, allowing template arg deduction
        template<template<typename, typename> typename Container, typename T, typename Allocator>
            void insert_many_records (const Container<T, Allocator>& records, size_t batch_size = 10)
            { return insert_many_records<T>(records, batch_size); }

        // matches std::array, allowing template arg deduction
        template<template<typename, auto> typename Container, typename T, auto size>
            void insert_many_records (const Container<T, size>& records, size_t batch_size = 10)
            { return insert_many_records<T>(records, batch_size); }

        template<typename T, IndexableContainer<T> Container>
            void insert_many_records (const Container& records, size_t batch_size = 10);

        template<class T, typename PrimaryKeyType>
            [[nodiscard]] std::optional<T> find_record(const PrimaryKeyType& id);

        template<class T, typename PrimaryKeyType>
            void delete_record(const PrimaryKeyType& id);

        template<class Select, typename From=void, class... Clauses>
            [[nodiscard]] auto select_query();

        template<class From>
            [[nodiscard]] auto delete_query();


        template<class T>
        [[nodiscard]] auto first();

        template<class T>
        [[nodiscard]] auto last();

        template<class T>
        void truncate();
    };

    template <class... Table>
    Connection<Table...> Connection<Table...>::create(
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
            auto err = ConnectionError("Unable to open sqlite connection", db_handle);
            if (logger) {
                logger(log_level::Error, err);
            }
            throw err;
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
                auto err = ConnectionError("Unable to destroy connection", handle);
                if (logger) {
                    logger(log_level::Error, err);
                }
                throw err;
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
    template<class C, bool distinct>
    struct Connection<Table...>::table_for_class<Count<C, distinct>> : std::type_identity<
        typename table_for_class<C>::type
    > {};

    template <class... Table>
    template<typename Field, bool distinct>
    requires (is_field<Field>)
    struct Connection<Table...>::table_for_class<Count<Field, distinct>> : std::type_identity<
        typename table_for_class<typename Field::table_t>::type
    > {};

    // I don't know why explicit specialization won't work here
    // but it doesn't, so here is a dumb workaround
    template <class... Table>
    template<class T>
    requires(std::is_same_v<T, CountAll>)
    struct Connection<Table...>::table_for_class<T> : std::type_identity<void> {};

    template <class... Table>
    template<class T, FixedLengthString name>
    struct Connection<Table...>::table_for_class<Field<T, name>> : table_for_class<T> {};

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

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename T>
    struct Connection<Table...>::selection_for_clause : std::type_identity<
        __selection<results_are_optional || selection_is_optional_v<table_for_class_t<T>::name, selectables_tuple>, table_for_class_t<T>>
    >{};

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename T, FixedLengthString field_name>
    struct Connection<Table...>::selection_for_clause<results_are_optional, selectables_tuple, From, Field<T, field_name>>: std::type_identity<
        __selection<results_are_optional || selection_is_optional_v<T::name, selectables_tuple>, Field<T, field_name>>
    >{};

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename T, bool distinct>
    struct Connection<Table...>::selection_for_clause<results_are_optional, selectables_tuple, From, Count<T, distinct>> {
        using table_t = table_for_class_t<T>;
        static_assert(table_t::has_primary_key,
                "Table must have a primary key in order to deduce the `Count` column. "
                "Use `CountAll` or a specific Field instead."
                );
        using pk_t = typename table_t::primary_key_t;
        using type = __count<Field<table_t, pk_t::name>, distinct>;
    };

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename Field, bool distinct>
    requires(is_field<Field>)
    struct Connection<Table...>::selection_for_clause<results_are_optional, selectables_tuple, From, Count<Field, distinct>> : std::type_identity<
        __count<Field, distinct>
    >{};

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From>
    struct Connection<Table...>::selection_for_clause<results_are_optional, selectables_tuple, From, CountAll> : std::type_identity<
        __count_all<From>
    >{};

    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename T>
    struct Connection<Table...>::select_type : std::type_identity<
        __select_impl<
            table_for_class_t<From>,
            typename selection_for_clause<results_are_optional, selectables_tuple, From, T>::type
        >
    > {};

    // This is the specialization for when dealing with a `Select<>` clause
    template <class... Table>
    template<bool results_are_optional, typename selectables_tuple, typename From, typename... U>
    struct Connection<Table...>::select_type<results_are_optional, selectables_tuple, From, Select<U...>> {
        using type = __select_impl<
            table_for_class_t<From>,
            typename selection_for_clause<results_are_optional, selectables_tuple, From, U>::type...
        >;
    };


    template <class... Table>
    template<typename T, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type : std::false_type {};

    template <class... Table>
    template <typename T, join_type_t type, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type<Join<T, type>, OtherTablesTuple> : std::type_identity<
        __join_impl<table_for_class_t<T>, type, OtherTablesTuple>
    >{};

    template <class... Table>
    template<typename FieldA, typename FieldB, join_type_t type, typename OtherTablesTuple>
    struct Connection<Table...>::join_clause_type<JoinOn<FieldA, FieldB, type>, OtherTablesTuple> : std::type_identity<
        __join_on_impl<FieldA, FieldB, type, OtherTablesTuple>
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

    // specialization to unwrap tuple
    template <class... Table>
    template <typename JoinedTables, typename JoinedClauses, typename... T>
    struct Connection<Table...>::make_joins<JoinedTables, JoinedClauses, std::tuple<T...>> :
        make_joins<JoinedTables, JoinedClauses, T...> {};

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

    template <FixedLengthString name, typename T>
    struct table_in_selectables;
    template <FixedLengthString name, typename... T>
    struct table_in_selectables<name, std::tuple<T...>> : std::bool_constant<(... || (T::table_name == name))> {};

    template <class... Table>
    template <typename selectables_tuple, typename T>
    struct Connection<Table...>::selections_are_selectable : std::bool_constant<
        table_in_selectables<table_for_class_t<T>::name, selectables_tuple>::value
    > {};

    template <class... Table>
    template <typename selectables_tuple, typename... T>
    struct Connection<Table...>::selections_are_selectable<selectables_tuple, Select<T...>> : std::bool_constant<
        (... && (table_in_selectables<table_for_class_t<T>::name, selectables_tuple>::value))
    > {};

    template <typename T>
    struct unwrap_from : std::type_identity<T> {};
    template <typename T>
    struct unwrap_from<From<T>> : std::type_identity<T> {};

    template<typename T>
    static constexpr bool select_starts_with_count_all = false;
    template<typename... U>
    static constexpr bool select_starts_with_count_all<Select<CountAll, U...>> = true;

    template <class... Table>
    template<typename SelectOrTable, typename From, typename... Clauses>
    auto Connection<Table...>::make_select_query()
    {
        // The first template argument should be a `Select` template,
        // or a table (or equivalent),
        // or a `Count` template
        // or a `Field` template
        static_assert(index_of_table<SelectOrTable>::value >= 0
                || is_select_v<SelectOrTable>
                || is_count<SelectOrTable>
                || is_field<SelectOrTable>,
            "The first template argument for select queries should be a table to select, "
            "or a `Select` template containing multiple tables");

        // because the `From` might be omitted, `From` could actually be another clause, or could be `void`
        using from_unwrapped = typename unwrap_from<From>::type;

        static constexpr bool select_clause_present = is_select_v<SelectOrTable>;
        static constexpr bool from_is_join = is_join<from_unwrapped>;

        static constexpr bool from_clause_present = !from_is_join &&
                                                    !std::is_void_v<from_unwrapped>;

        static constexpr bool select_is_count_all = std::is_same_v<SelectOrTable, CountAll> || select_starts_with_count_all<SelectOrTable>;

        static_assert(not select_is_count_all || from_clause_present,
                "`CountAll` only makes sense with a `From` clause"
                );

        if constexpr (from_clause_present) {
            static_assert(select_clause_present || select_is_count_all,
                    "A `From` clause should only be supplied if `Select` or `CountAll` is used clause is used");
        }

        // if there is a `From` then the `from_t` is clear,
        // otherwise it should be deduced from the first thing selected
        using from_t = std::conditional_t<from_clause_present,
            from_unwrapped,
            typename select_or_table_table<SelectOrTable>::type
        >;

        static_assert(all_of<(is_join<Clauses>)...>,
                "select `Clauses` should be `Join` or `JoinOn` templates");

        using joins_t = std::conditional_t<from_is_join,
            std::tuple<from_unwrapped, Clauses...>,
            std::tuple<Clauses...>
        >;

        // if we have joins in the query, handle them here:
        if constexpr (std::tuple_size_v<joins_t>)
        {
            // digest the joins using the `make_joins` meta function
            // accounting for the `From` actually being a join statement in disguise
            using join_meta = make_joins<
                std::tuple<table_for_class_t<from_t>>,
                std::tuple<>,
                joins_t
            >;

            using joins_tuple = typename join_meta::type;
            static constexpr bool query_is_nested = join_meta::query_is_nested;

            // figure out what tables are present in the query based on the joins
            using selectables_t = typename selectables_from_joins<joins_tuple>::type;

            // ensure the selections make sense
            static_assert(selections_are_selectable<selectables_t, SelectOrTable>::value,
                    "One of the selected tables is not present in the query.");

            // if the query is nested and ANY of the joins produce optional records,
            // we make every column optional, figuring out specifically which columns are optional
            // is too complicated
            static constexpr bool results_are_optional = query_is_nested && a_selection_is_optional<selectables_t>::value;
            return SelectQuery<
                selectables_t,
                typename select_type<results_are_optional, selectables_t, from_t, SelectOrTable>::type,
                joins_tuple>
            (
                _db_handle.get(),
                _logger
            );

        }
        // queries without joins are handled here:
        else
        {
            // only the `from_t` is selectable since there are no joins
            using selectables_t = std::tuple<__selectable_table<false, table_for_class_t<from_t>::name>>;

            return SelectQuery<selectables_t, typename select_type<false, std::tuple<>, from_t, SelectOrTable>::type> (
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
    void Connection<Table...>::exec(const std::string& query)
    {
        auto stmt = make_statement(query);
        stmt.step();
    }

    template <class... Table>
    void Connection<Table...>::log_error(const Error& err)
    {
        _logger(log_level::Error, err);
    }

    template <class... Table>
    void Connection<Table...>::transaction(std::function<void()> run)
    {
        exec("BEGIN TRANSACTION;");
        try {
            run();
        } catch(const zxorm::Error& err) {
            exec("ROLLBACK TRANSACTION;");
            throw;
        }

        exec("COMMIT TRANSACTION;");
    };

    template <class... Table>
    template<class T>
    void Connection<Table...>::update_record (const T& record)
    {
        using table_t = table_for_class_t<T>;
        static_assert(table_t::has_primary_key, "Cannot execute an update on a table without a primary key");

        auto& pk = table_t::primary_key_t::getter(record);

        if constexpr (table_has_rowid<T>()) {
            if (!pk) {
                throw Error("Cannot update record with unknown rowid");
            }
        }

        auto& stmt = _update_statement_cache[table_t::name.value];
        if (!stmt) {
            Statement update_stmt = make_statement(table_t::update_query());
            stmt = std::make_shared<Statement>(std::move(update_stmt));
        } else {
            stmt->reset();
        }

        int i = 1;
        std::apply([&]<typename... U>(const U&...) {
            ([&]() {
                if constexpr (not U::is_primary_key) {
                    auto& val = U::getter(record);
                    stmt->bind(i++, val);
                }

            }(), ...);
        }, typename table_t::columns_t{});

        stmt->bind(i++, pk);
        stmt->step();

        if (!stmt->done()) [[unlikely]] {
            throw Error("Update query didn't run to completion");
        }
    }

    template <class... Table>
    template<typename T, IndexableContainer<T> Container>
    void Connection<Table...>::insert_many_records (const Container& records, size_t batch_size)
    {
        using table_t = table_for_class_t<T>;
        return transaction([&]() {
            size_t inserted = 0;
            // this should always be set
            std::optional<Statement> insert_stmt;

            while (inserted < records.size()) {
                // iteration is less than batch size, query needs to be [re-]initialized
                if (records.size() - inserted < batch_size) {
                    batch_size = records.size() - inserted;
                    auto query = table_t::insert_query(batch_size);
                    insert_stmt = make_statement(query);
                }
                // first itertion, initiaize statment
                else if (inserted == 0) {
                    auto query = table_t::insert_query(batch_size);
                    insert_stmt = make_statement(query);
                // subsequent iterations just reset the same query
                } else {
                    insert_stmt.value().reset();
                }


                int i = 1;
                for (size_t row = 0; row < batch_size; row++) {
                    std::apply([&]<typename... U>(const U&...) {
                        ([&]() {
                            if constexpr (!U::is_auto_inc_column) {
                                const auto& val = U::getter(records[row + inserted]);
                                insert_stmt.value().bind(i++, val);
                            }

                        }(), ...);
                    }, typename table_t::columns_t{});
                }

                insert_stmt.value().step();

                if (!insert_stmt.value().done()) [[unlikely]] {
                    throw Error("Insert query didn't run to completion");
                }

                inserted += batch_size;
            }
        });
    }

    template <class... Table>
    template<class T>
    void Connection<Table...>::insert_record_impl (
        std::conditional_t<table_has_rowid<T>(), T&, const T&> record)
    {
        using table_t = table_for_class_t<T>;

        auto& stmt = _insert_statement_cache[table_t::name.value];

        if (!stmt) {
            auto query = table_t::insert_query();
            Statement insert_stmt = make_statement(query);
            stmt = std::make_shared<Statement>(std::move(insert_stmt));
        } else {
            stmt->reset();
        }


        int i = 1;
        std::apply([&]<typename... U>(const U&...) {
            ([&]() {
                if constexpr (!U::is_auto_inc_column) {
                    auto& val = U::getter(record);
                    stmt->bind(i++, val);
                }

            }(), ...);
        }, typename table_t::columns_t{});

        stmt->step();

        if (!stmt->done()) [[unlikely]] {
            throw Error("Insert query didn't run to completion");
        }

        if constexpr (table_has_rowid<T>()) {
            int64_t value = sqlite3_last_insert_rowid(_db_handle.get());
            table_t::primary_key_t::setter(record, value);
        }
    }

    template <class... Table>
    void Connection<Table...>::create_tables(bool if_not_exist)
    {
        std::array<Statement,  sizeof...(Table)> statements =
            { make_statement(Table::create_table_query(if_not_exist))... };

        return transaction([&]() {
            for (auto& s : statements) {
                s.step();
            }
        });
    }

    template <class... Table>
    size_t Connection<Table...>::count_tables()
    {
        auto count_stmt = make_statement("SELECT COUNT(*) FROM `sqlite_schema` WHERE `type` = 'table';");

        count_stmt.step();

        ssize_t count;
        count_stmt.read_column(0, count);
        return count;
    }

    template <class... Table>
    template<class T, typename PrimaryKeyType>
    std::optional<T> Connection<Table...>::find_record(const PrimaryKeyType& id)
    {
        using table_t = table_for_class_t<T>;
        static_assert(table_t::has_primary_key, "Cannot execute a find on a table without a primary key");

        using primary_key_t = typename table_t::primary_key_t;

        static_assert(std::is_convertible_v<PrimaryKeyType, typename primary_key_t::member_t>,
                "Primary key type does not match the type specified in the definition of the table");

        const char* table_name = table_t::name.value;
        auto& cache_item = _query_cache[table_name][QueryCacheType::find_record];

        auto e = Field<table_t, primary_key_t::name>() == id;
        using query_t = decltype(make_select_query<T>().where_one(e));

        if (!cache_item) {
            auto query = make_select_query<T>().where_one(e);
            cache_item = std::make_shared<query_t>(std::move(query));
        } else {
            std::static_pointer_cast<query_t>(cache_item)->rebind(id);
        }

        return std::static_pointer_cast<query_t>(cache_item)->exec();
    }

    template <class... Table>
    template<class T, typename PrimaryKeyType>
    void Connection<Table...>::delete_record(const PrimaryKeyType& id)
    {
        using table_t = table_for_class_t<T>;

        static_assert(table_t::has_primary_key, "Cannot execute a delete on a table without a primary key");

        using primary_key_t = typename table_t::primary_key_t;

        static_assert(std::is_convertible_v<PrimaryKeyType, typename primary_key_t::member_t>,
                "Primary key type does not match the type specified in the definition of the table");

        const char* table_name = table_t::name.value;
        auto& cache_item = _query_cache[table_name][QueryCacheType::delete_record];

        auto e = Field<table_t, primary_key_t::name>() == id;
        using query_t = decltype(make_delete_query<T>().where(e));

        if (!cache_item) {
            auto query = make_delete_query<T>().where(e);
            cache_item = std::make_shared<query_t>(std::move(query));
        } else {
            std::static_pointer_cast<query_t>(cache_item)->rebind(id);
        }

        return std::static_pointer_cast<query_t>(cache_item)->exec();
    }

    template <class... Table>
    template<class From>
    auto Connection<Table...>::delete_query()
    {
        return make_delete_query<From>();
    }

    template <class... Table>
    template<typename Select, typename From, typename... Clauses>
    auto Connection<Table...>::select_query()
    {
        return make_select_query<Select, From, Clauses...>();
    }

    template <class... Table>
    template<class T>
    auto Connection<Table...>::first()
    {
        using table_t = table_for_class_t<T>;
        using query_t = decltype(make_select_query<T>().one());

        auto& cache_item = _query_cache[table_t::name.value][QueryCacheType::first];

        if (!cache_item) {
            auto query = make_select_query<T>().one();
            cache_item = std::make_shared<query_t>(std::move(query));
        }

        return std::static_pointer_cast<query_t>(cache_item)->exec();
    }

    template <class... Table>
    template<class T>
    auto Connection<Table...>::last()
    {
        using table_t = table_for_class_t<T>;
        using query_t = decltype(make_select_query<T>().one());
        using pk_field = Field<table_t, table_t::primary_key_t::name>;

        auto& cache_item = _query_cache[table_t::name.value][QueryCacheType::last];

        if (!cache_item) {
            auto query = make_select_query<T>().template order_by<pk_field>(order_t::DESC).one();
            cache_item = std::make_shared<query_t>(std::move(query));
        }

        return std::static_pointer_cast<query_t>(cache_item)->exec();
    }

    template <class... Table>
    template<class T>
    void Connection<Table...>::truncate()
    {
        using table_t = table_for_class_t<T>;
        exec(std::string("DELETE FROM `") + table_t::name.value + "`;");
    }
};
