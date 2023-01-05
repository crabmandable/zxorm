#pragma once
namespace zxorm {
    // base class so that we can deal with any clause without having to know how it binds
    struct BindingClauseBase {
        std::string clause;
        BindingClauseBase(std::string clause) : clause{clause} {}
        virtual OptionalError bind(Statement& s) = 0;
    };

    template <typename Bindings = std::tuple<>>
    struct BindingClause: public BindingClauseBase {
        Bindings bindings;

        BindingClause(std::string clause, Bindings bindings) : BindingClauseBase{clause}, bindings{bindings} {}

        OptionalError bind(Statement& s) {
            return s.bind(bindings);
        }
    };

    template <typename Bindings = std::tuple<>>
    struct Where: public BindingClause<Bindings> {
        template <typename Expression>
        Where(const Expression& e) : BindingClause<Bindings>{std::string("WHERE ") + e.serialize(), e.bindings()} {}
    };

    template <typename From, typename... U>
    struct __select_impl {
        using from_t = From;
        using tables_t = std::tuple<From, U...>;

        friend std::ostream & operator<< (std::ostream &out, const __select_impl<From, U...>&) {
            out << "SELECT ";
                std::apply([&](const auto&... a) {
                    ([&]() {
                        using table_t = std::remove_reference_t<decltype(a)>;
                        out << "`" << table_t::name << "`.*,";
                     }(), ...);
                }, tables_t{});
            // erase trailing comma
            out.seekp(-1, std::ios_base::end);
            return out;
        }
    };

    template <typename From, typename... U>
    struct Select {};

    template <typename T>
    struct is_select : std::false_type {};

    template <typename From, typename... U>
    struct is_select<Select<From, U...>> : std::true_type {};

    template <typename T>
    static constexpr bool is_select_v = is_select<T>::value;
};
