#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query/query.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/table.hpp"

namespace zxorm {
    template <typename SelectablesTuple, typename Select, typename JoinsTuple=std::tuple<>>
    class SelectQuery : public Query<SelectablesTuple, typename Select::from_t, Select, JoinsTuple> {
    private:
        using Super = Query<SelectablesTuple, typename Select::from_t, Select, JoinsTuple>;

        std::string _limit_clause;
        std::string _order_clause;
        std::string _group_by_clause;

        virtual void serialize_limits(std::ostream& ss) override {
            ss << _group_by_clause << " " << _order_clause << " " << _limit_clause;
        }

        template <typename T, size_t s>
        struct ColumnOffset{
            using type = T;
            static constexpr size_t offset = s;
        };


        template <size_t idx, typename... Ts>
        struct find_offset {
        private:
            static constexpr std::array<size_t, sizeof...(Ts)> _n_columns = { Ts::n_columns... };

            static constexpr size_t _sum (size_t i)
            {
                if (i >= idx || i >= sizeof...(Ts)) {
                    return 0;
                }
                return _n_columns[i] + _sum(i+1U);
            }

        public:
            static constexpr size_t value = _sum(0);
        };

        template<typename Target, typename ListHead, typename... ListTails>
        static constexpr size_t find_index_of_type()
        {
            if constexpr (std::is_same<Target, ListHead>::value)
                return 0;
            else
                return 1 + find_index_of_type<Target, ListTails...>();
        }

        /**
         * with_offsets - metafunction that transforms the selections into
         *                `ColumnOffset`s that include the offset to read from
         *                when loading each row of query results
         */
        template <typename T>
        struct with_offsets : std::false_type {}; // unused base case

        template <typename... T>
        struct with_offsets <std::tuple<T...>> {
            template <typename Needle, typename... Haystack>
            struct elem : std::type_identity<
                ColumnOffset<Needle, find_offset<find_index_of_type<Needle, Haystack...>(), Haystack...>::value>
            > { };

            using type = std::tuple<typename elem<T, T...>::type...>;
        };

        template <typename T>
        using with_offsets_t = typename with_offsets<T>::type;

        using return_t = typename Select::return_t;

        using result_tuple_t = typename Select::result_t;

        template<size_t record_index, typename T>
        requires (std::tuple_element_t<record_index, typename Select::selections_tuple>::is_optional)
        static auto row_res_to_row (const OptionalResult<T>& row_res) -> std::optional<T> {
            if (row_res.has_value()) {
                return row_res.value();
            }

            return std::nullopt;
        }

        template<size_t record_index, typename T>
        static auto row_res_to_row (const OptionalResult<T>& row_res) -> T {
            return row_res.value();
        }

        static auto read_row(Statement& s) -> Result<return_t>
        {
            if constexpr (std::tuple_size_v<typename Select::selections_tuple> == 1) {
                using selection = std::tuple_element_t<0, typename Select::selections_tuple>;
                return selection::get_row(s);
            } else {
                auto us_res = std::apply([&](const auto&... a) {
                    auto get_row = [&]<typename Pair>(const Pair&) {
                        // The pair here is a ColumnOffset template
                        using selection_t = typename Pair::type;
                        using row_t = typename selection_t::result_t;

                        constexpr size_t offset = Pair::offset;
                        auto row = selection_t::get_row(s, offset);

                        if (row.is_error()) return row_t{std::move(row)};

                        if (selection_t::row_is_null(row.value()))
                        {
                            return row_t{std::nullopt};
                        }

                        return row_t{std::move(row)};
                    };


                    return result_tuple_t {
                        get_row(a)...
                    };
                }, with_offsets_t<typename Select::selections_tuple>{});


                OptionalResult<return_t> us = std::nullopt;

                std::apply([&](const auto&... a) {
                    // if an error is in here we should return it
                    ([&]() {
                        if (!us.is_error() && a.is_error()) {
                            us = a.error();
                        }
                     }(), ...);

                    // if there is no error, set the values
                    if (!us.is_error()) {
                        us = { row_res_to_row<tuple_index<decltype(a), std::tuple<decltype(a)...>>::value>(a)... };
                    }
                }, us_res);

                if (us.is_error()) {
                    return us.error();
                }

                return us.value();
            }
        }

    public:
        SelectQuery(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        SelectQuery(SelectQuery&& other) = default;

        auto& where(auto&&... args) {
            Super::where(std::forward<decltype(args)>(args)...);
            return *this;
        }

        auto& limit(unsigned long limit, unsigned long offset = 0) {
            if (_limit_clause.empty()) {
                std::stringstream ss;
                ss << "LIMIT " << limit;
                if (offset) {
                    ss << " OFFSET " << offset;
                }
                _limit_clause = ss.str();
            }
            return *this;
        }

        template <typename Field>
        requires(is_field<Field>)
        auto& order_by(order_t ord = order_t::ASC) {
            if (_order_clause.empty()) {
                using column_t = typename Field::column_t;
                using table_t = typename Field::table_t;

                static_assert(table_is_selectable<table_t, SelectablesTuple>::value,
                        "Field for `order_by` must belong to a table present in the query");

                std::stringstream ss;
                ss << "ORDER BY `" << table_t::name.value << "`.`" << column_t::name.value << "` " << ord;
                _order_clause = ss.str();
            }
            return *this;
        }

        template <typename T>
        auto& group_by() {
            if (_group_by_clause.empty()) {
                const char* column_name;
                const char* table_name;

                static_assert(is_field<T> || is_table_v<T>,
                        "Argument for `group_by` should be a Field or a Table");

                if constexpr (is_field<T>) {
                    column_name = T::column_t::name.value;
                    table_name = T::table_t::name.value;
                    static_assert(table_is_selectable<typename T::table_t, SelectablesTuple>::value,
                            "Field for `group_by` must belong to a table present in the query");
                } else {
                    column_name = T::primary_key_t::name.value;
                    table_name = T::name.value;
                    static_assert(table_is_selectable<T, SelectablesTuple>::value,
                            "Table for `group_by` must be a table present in the query");
                }


                std::stringstream ss;
                ss << "GROUP BY `" << table_name << "`.`" << column_name << "`";
                _group_by_clause = ss.str();
            }
            return *this;
        }

        auto one() -> OptionalResult<return_t>
        {
            limit(1);
            ZXORM_TRY(Super::prepare());
            ZXORM_TRY(Super::_stmt->step());
            if (Super::_stmt->done()) {
                return std::nullopt;
            }

            return read_row(*Super::_stmt);
        }

        Result<RecordIterator<return_t>> many() {
            ZXORM_TRY(Super::prepare());
            return RecordIterator<return_t>(Super::_stmt, read_row);
        }
    };
};
