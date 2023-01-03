#pragma once
#include <sqlite3.h>
#include "zxorm/common.hpp"
#include "zxorm/result.hpp"
#include "zxorm/orm/query.hpp"
#include "zxorm/orm/record_iterator.hpp"

namespace zxorm {
    namespace __select_detail {
        template <typename From, typename... U>
        struct SelectColumnClause {
            friend std::ostream & operator<< (std::ostream &out, const SelectColumnClause<From, U...>&) {
                out << "SELECT ";
                out << "`" << From::name << "`.* ";
                    std::apply([&](const auto&... a) {
                        ([&]() {
                            using table_t = std::remove_reference_t<decltype(a)>;
                            out << ", ";
                            out << "`" << table_t::name << "`.* ";
                         }(), ...);
                    }, std::tuple<U...>{});
                return out;
            }
        };

        template <typename T, size_t s>
        struct ColumnOffset{
            using type = T;
            static constexpr size_t offset = s;
        };


        template <size_t idx, typename... Ts>
        struct find_offset {
        private:
            static constexpr std::array<size_t, sizeof...(Ts)> _n_columns = { Ts::n_columns... };

            static constexpr size_t _sum (size_t i = 0U)
            {
                if (i >= idx || i >= sizeof...(Ts)) {
                    return 0;
                }
                return _n_columns[i] + _sum(i+1U);
            }

        public:
            static constexpr size_t value = _sum();
        };

        template<typename Target, typename ListHead, typename... ListTails>
        constexpr size_t find_index_of_type()
        {
            if constexpr (std::is_same<Target, ListHead>::value)
                return 0;
            else
                return 1 + find_index_of_type<Target, ListTails...>();
        };

        template <typename... Ts>
        using with_offsets  = std::tuple<ColumnOffset<Ts, find_offset<find_index_of_type<Ts, Ts...>(), Ts...>::value>...>;

    };

    template <class From, class... U>
    class Select : public Query<From, __select_detail::SelectColumnClause<From, U...>> {
    private:
        using Super = Query<From, __select_detail::SelectColumnClause<From, U...>>;

        std::string _limit_clause;
        std::string _order_clause;

        virtual void serialize_limits(std::ostream& ss) override {
            ss << _order_clause << " " << _limit_clause;
        }

        static auto read_row(Statement& s) -> Result<std::conditional_t<
                (sizeof...(U) > 0),
                std::tuple<typename From::object_class, typename U::object_class...>,
                typename From::object_class
            >>
        {
            if constexpr (sizeof...(U) == 0) {
                return From::get_row(s);
            } else {
                ZXORM_GET_RESULT(auto f, From::get_row(s));

                auto us_res = std::apply([&](const auto&... a) {
                    auto get_row = [&](const auto& pair) {
                        using pair_t = std::remove_reference_t<decltype(pair)>;
                        using table_t = pair_t::type;
                        constexpr size_t offset = From::n_columns + pair_t::offset;
                        return table_t::get_row(s, offset);
                    };

                    return std::tuple<Result<typename U::object_class>...>{
                        get_row(a)...
                    };
                }, __select_detail::with_offsets<U...>{});


                OptionalResult<std::tuple<typename U::object_class...>> us = std::nullopt;

                std::apply([&](const auto&... a) {
                    // if an error is in here we should return it
                    ([&]() {
                        if (!us.is_error() && a.is_error()) {
                            us = a.error();
                        }
                     }(), ...);

                    // if there is no error, set the values
                    if (!us.is_error()) {
                        us = { a.value()... };
                    }
                }, us_res);

                if (us.is_error()) {
                    return us.error();
                }

                return std::tuple_cat(std::tuple(f), us.value());
            }
        }

    public:
        Select(sqlite3* handle, Logger logger) :
            Super(handle, logger) {}

        auto where(auto&&... args) {
            Super::where(std::forward<decltype(args)>(args)...);
            return *this;
        }

        template <Field field_a, Field field_b>
        auto join(join_type_t type = join_type_t::INNER) {
            Super::template join<field_a, field_b>(type);
            return *this;
        }

        template <FixedLengthString foreign_table>
        auto join(join_type_t type = join_type_t::INNER) {
            Super::template join<foreign_table>(type);
            return *this;
        }

        auto limit(unsigned long limit) {
            std::stringstream ss;
            ss << "LIMIT " << limit;
            _limit_clause = ss.str();
            return *this;
        }

        template <FixedLengthString field>
        auto order_by(order_t ord) {
            static_assert(not std::is_same_v<typename From::column_by_name<field>::type, std::false_type>,
                "ORDER BY clause must use a field beloning to the Table"
            );
            std::stringstream ss;
            ss << "ORDER BY `" << field.value << "` " << ord;
            _order_clause = ss.str();
            return *this;
        }

        // TODO: depending on the join type some of the tuple values could be null
        auto one() -> OptionalResult<std::conditional_t<
                (sizeof...(U) > 0),
                std::tuple<typename From::object_class, typename U::object_class...>,
                typename From::object_class
            >>
        {
            assert(_limit_clause.empty());
            limit(1);
            ZXORM_GET_RESULT(Statement s, Super::prepare());
            ZXORM_TRY(s.step());
            if (s.done()) {
                return std::nullopt;
            }

            return read_row(s);
        }

        Result<RecordIterator<From, U...>> many() {
            auto s = Super::prepare();
            if (s.is_error()) {
                return s.error();
            }
            return RecordIterator<From, U...>(std::move(s.value()), read_row);
        }
    };
};
