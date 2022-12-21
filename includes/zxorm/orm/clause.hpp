#pragma once
namespace zxorm {
    // base class so that we can deal with any clause without having to know how it binds
    struct ClauseBase {
        std::string clause;
        ClauseBase(std::string clause) : clause{clause} {}
        virtual OptionalError bind(Statement& s) = 0;
    };

    template <typename Bindings = std::tuple<>>
    struct Clause: public ClauseBase {
        Bindings bindings;

        Clause(std::string clause, Bindings bindings) : ClauseBase{clause}, bindings{bindings} {}

        OptionalError bind(Statement& s) {
            return s.bind(bindings);
        }
    };

    template <typename Bindings = std::tuple<>>
    struct Where: public Clause<Bindings> {
        template <typename Expression>
        Where(const Expression& e) : Clause<Bindings>{std::string("WHERE ") + e.serialize(), e.bindings()} {}
    };
};
