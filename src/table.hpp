#pragma once
#include <optional>
#include "common.hpp"

namespace zorm {
template <class T, class... Column>
class Table {
    public:
        void printColumns(T obj) {
            ([&]
             {
                std::cout << "column: " << Column::getter(obj) << std::endl;
             }(), ...);
        }

        std::optional<Error> create() { return std::nullopt; }
};
};

template <typename C, typename T, T (C::*Getter)(), void (C::*Setter)(T) >
class ColumnPrivate {
    public:
    static auto getter(C obj) { return (obj.*Getter)(); };
    static void setter(C obj, T arg) { obj.*Setter(arg); };
};

template <typename C, typename T, T (C::*value)>
class Column {
    public:
    static auto getter(C obj) { return obj.*value; };
    static void setter(C obj, T arg) { obj.*value = arg; };
};

