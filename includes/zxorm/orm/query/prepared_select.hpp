#pragma once


#include <memory>
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query/query.hpp"
#include "zxorm/orm/record_iterator.hpp"
#include "zxorm/orm/table.hpp"

namespace zxorm {
    template <class Select, class Bindings = std::tuple<>>
    class BasePreparedSelect: public BasePreparedQuery {
    public:
        BasePreparedSelect(BasePreparedSelect&& old) = default;
    protected:
        std::shared_ptr<Statement> _stmt;
        Bindings _bindings;

        BasePreparedSelect(std::shared_ptr<Statement> stmt): _stmt(stmt) {};

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


                    return typename Select::result_t {
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
        OptionalError rebind(auto&&... bindings) {
            _bindings = Bindings{std::forward<decltype(bindings)>(bindings)...};

            ZXORM_TRY(_stmt->reset());
            ZXORM_TRY(_stmt->bind(_bindings));
            return std::nullopt;
        }
    };

    template <class Select, class Bindings = std::tuple<>>
    class PreparedSelectOne: public BasePreparedSelect<Select, Bindings> {
        using Super = BasePreparedSelect<Select, Bindings>;
    public:
        PreparedSelectOne(std::shared_ptr<Statement> stmt): Super(stmt) {};
        PreparedSelectOne(PreparedSelectOne&&) = default;

        auto exec() -> OptionalResult<typename Select::return_t> {
            ZXORM_TRY(Super::_stmt->rewind());
            ZXORM_TRY(Super::_stmt->step());
            if (Super::_stmt->done()) {
                return std::nullopt;
            }

            return Super::read_row(*Super::_stmt);
        }
    };

    template <class Select, class Bindings = std::tuple<>>
    class PreparedSelectMany: public BasePreparedSelect<Select, Bindings> {
        using Super = BasePreparedSelect<Select, Bindings>;

    public:
        PreparedSelectMany(std::shared_ptr<Statement> stmt): Super(stmt) {};
        PreparedSelectMany(PreparedSelectMany&&) = default;

        auto exec() -> Result<RecordIterator<typename Select::return_t>>
        {
            ZXORM_TRY(Super::_stmt->rewind());
            return RecordIterator<typename Select::return_t>(Super::_stmt, Super::read_row);
        }
    };
};
