#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include "zxorm/zxorm.hpp"

using namespace zxorm;

void logger(LogLevel level, const char* msg) {
    std::cout << "connection logger (" << (int)level << "): ";
    std::cout << msg << std::endl;
}

class TableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto createdConn = Connection::create("test.db", 0, 0, &logger);
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
    int _id = 0;
    std::string _name;
    auto getId() { return _id; }
    void setId(int id) { _id = id; }
    auto getName() { return _name; }
    void setName(std::string name) { _name = name; }
};

using table_t = Table<"test", Object,
    Column<"id", &Object::_id>,
    Column<"name", &Object::_name>
        > ;

using tablepriv_t = Table<"test", Object,
    ColumnPrivate<"id", &Object::getId, &Object::setId>,
    ColumnPrivate<"name", &Object::getName, &Object::setName>
        > ;

TEST_F(TableTest, Columns) {
    table_t table;

    ASSERT_EQ(table.columnName(0), "id");
    ASSERT_EQ(table.columnName(1), "name");
}

TEST_F(TableTest, ColumnsPrivate) {
    tablepriv_t table;
    ASSERT_EQ(table.columnName(0), "id");
    ASSERT_EQ(table.columnName(1), "name");
}

TEST_F(TableTest, nColumns) {
    ASSERT_EQ(table_t::nColumns(), 2);
}

TEST_F(TableTest, CreateTableQuery) {
    std::string query = table_t::createTableQuery();
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    ASSERT_EQ(trimmed, "CREATE TABLE test ( id INT, name TEXT );");
    std::string same = tablepriv_t::createTableQuery();
    ASSERT_EQ(same, query);
}

TEST_F(TableTest, dbg) {
    table_t::printTable();
    tablepriv_t::printTable();
}
