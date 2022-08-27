#pragma once
#include <optional>
#include "common.hpp"
#include <algorithm>

namespace zorm {
template <class T, class... Column>
class Table {
    public:
        void printColumns(T obj) {
            ([&]
             {
                std::cout << "column " << Column::name() << ": " << Column::getter(obj) << std::endl;
             }(), ...);
        }

        std::optional<Error> create() { return std::nullopt; }
};
};

template<size_t N>
struct ColumnName {
    constexpr ColumnName(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    char value[N];
};

template <ColumnName column_name, typename C, typename T, T (C::*Getter)(), void (C::*Setter)(T) >
class ColumnPrivate {
    public:
    static constexpr const char* name() { return column_name.value; }
    static auto getter(C obj) { return (obj.*Getter)(); };
    static void setter(C obj, T arg) { obj.*Setter(arg); };
};

template <ColumnName column_name, typename C, typename T, T (C::*value)>
class Column {
    public:
    static constexpr const char* name() { return column_name.value; }
    static auto getter(C obj) { return obj.*value; };
    static void setter(C obj, T arg) { obj.*value = arg; };
};
