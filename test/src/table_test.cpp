#include <gtest/gtest.h>
#include <filesystem>
#include "zxorm/zxorm.hpp"

using namespace zxorm;

void logger(LogLevel level, const char* msg) {
    std::cout << "connection logger (" << (int)level << "): ";
    std::cout << msg << std::endl;
}

class TableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto createdConn = Connection::Create("test.db", 0, 0, &logger);
    if (!std::holds_alternative<Connection>(createdConn)) {
        throw "Unable to open connection";
    }

     myConn = std::make_shared<Connection>(std::move(std::get<Connection>(createdConn)));
  }

  std::shared_ptr<Connection> myConn;

  void TearDown() override {
      myConn = nullptr;
      std::filesystem::remove("test.db");
  }
};

struct Object {
    int _id;
    std::string _name;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

TEST_F(TableTest, Columns) {
    Table<Object,
        Column<"id", &Object::_id>,
        Column<"name", &Object::_name>
            > table;

    Object o;
    o.setId(100);
    o.setName("Steve");
    table.printColumns(o);
}

TEST_F(TableTest, ColumnsPrivate) {
    Table<Object,
        ColumnPrivate<"id", &Object::getId, &Object::setId>,
        ColumnPrivate<"name", &Object::getName, &Object::setName>
            > table;
    Object o;
    o.setId(100);
    o.setName("Steve");
    table.printColumns(o);
}
