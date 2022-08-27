#include <iostream>
#include "connection.hpp"
#include "table.hpp"

void logger(zorm::LogLevel level, const char* msg) {
    std::cout << "connection logger (" << (int)level << "): ";
    std::cout << msg << std::endl;
}

struct User {
    int _id;
    std::string _name;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

int main (void) {
    using namespace zorm;
    auto createdConn = Connection::Create("test.db", 0, 0, &logger);

    if (!std::holds_alternative<Connection>(createdConn)) {
        std::cout << "fuck: " << std::get<Error>(createdConn) << std::endl;
        return -1;
    }

    Connection myConn = std::move(std::get<Connection>(createdConn));
    std::cout << "yeah" << std::endl;

    Table<User,
        ColumnPrivate<User, int, &User::getId, &User::setId>,
        ColumnPrivate<User, std::string, &User::getName, &User::setName>
            > tablePriv;
    User u;
    u.setId(100);
    u.setName("Steve");
    std::cout << "private columns:" << std::endl;
    tablePriv.printColumns(u);

    Table<User,
        Column<User, int, &User::_id>,
        Column<User, std::string, &User::_name>
            > table;

    std::cout << "public columns:" << std::endl;
    table.printColumns(u);
}
