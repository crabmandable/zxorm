#pragma once
namespace zxorm {
    struct WhereBase {
        std::string clause;
        WhereBase(std::string clause) : clause{std::string("WHERE ") + clause} {}
        virtual OptionalError bind(Statement& s) = 0;
    };

    template <typename Bindings = std::tuple<>>
    struct Where: public WhereBase {
        Bindings bindings;

        Where(std::string clause, Bindings bindings) : WhereBase{clause}, bindings{bindings} {}

        template <typename Expression>
        Where(const Expression& e) : WhereBase{e.serialize()}, bindings{e.bindings()} {}

        OptionalError bind(Statement& s) {
            return s.bind(bindings);
        }
    };
};
