#include <gtest/gtest.h>
#include <filesystem>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int id = 0;
    int someId = 0;
    std::string someText;
    float someFloat;
    std::optional<float> someOptional;
    std::optional<std::vector<char>> someOptionalBuffer;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::someText, Unique<conflict_t::replace>>,
    Column<"float", &Object::someFloat>,
    Column<"someId", &Object::someId >,
    Column<"someOptional", &Object::someOptional>,
    Column<"someOptionaBuffer", &Object::someOptionalBuffer>
        >;

using MyConnection = Connection<table_t>;

class QueryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto createdConn = MyConnection::create("test.db", 0, 0, &logger);
    if (!std::holds_alternative<MyConnection>(createdConn)) {
        throw "Unable to open connection";
    }

     myConn = std::make_shared<MyConnection>(std::move(std::get<MyConnection>(createdConn)));
  }

  std::shared_ptr<MyConnection> myConn;

  void TearDown() override {
      myConn = nullptr;
      std::filesystem::rename("test.db", "test.db.old");
  }
};

TEST_F(QueryTest, CreateTables) {
    auto err = myConn->createTables(false);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);
}

TEST_F(QueryTest, CreateIfExistsTables) {
    auto err = myConn->createTables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);

    err = myConn->createTables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);
}

TEST_F(QueryTest, FindNothing) {
    auto createTablesErr = myConn->createTables(true);
    ASSERT_FALSE(createTablesErr);

    auto result = myConn->find<Object>(1);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(result);
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        auto record = std::get<std::optional<Object>>(result);
        if (record.has_value()) {
            std::cout << record.value().id << std::endl;
            std::cout << record.value().someId << std::endl;
        }
        ASSERT_FALSE(record);
    }
}

TEST_F(QueryTest, InsertSomething) {
    auto err = myConn->createTables(true);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }
    ASSERT_FALSE(err);
    Object obj;

    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;

    err = myConn->insert(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }
    ASSERT_FALSE(err);
}

TEST_F(QueryTest, FindSomething) {
    auto err = myConn->createTables(true);
    ASSERT_FALSE(err);

    Object obj;
    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;

    err = myConn->insert(obj);
    ASSERT_FALSE(err);

    auto result = myConn->find<Object>(1);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(result);
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        auto record = std::get<std::optional<Object>>(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().someText, "Some text");
        ASSERT_FLOAT_EQ(record.value().someFloat, 3.14);
        ASSERT_EQ(record.value().someId, 42);
        ASSERT_FALSE(record.value().someOptional.has_value());
        ASSERT_FALSE(record.value().someOptionalBuffer.has_value());
    }
}

TEST_F(QueryTest, FindSomethingWithOptionalsFilled) {
    auto err = myConn->createTables(true);
    ASSERT_FALSE(err);

    Object obj;
    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;
    obj.someOptional = 42.333;
    obj.someOptionalBuffer = {'y', 'o'};

    err = myConn->insert(obj);
    ASSERT_FALSE(err);

    auto result = myConn->find<Object>(1);
    if (std::holds_alternative<Error>(result)) {
        auto err = std::get<Error>(result);
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        auto record = std::get<std::optional<Object>>(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().someText, "Some text");
        ASSERT_FLOAT_EQ(record.value().someFloat, 3.14);
        ASSERT_EQ(record.value().someId, 42);
        ASSERT_TRUE(record.value().someOptional.has_value());
        ASSERT_FLOAT_EQ(record.value().someOptional.value(), 42.333);
        auto expectedBuff = std::vector<char> {'y', 'o'};
        ASSERT_EQ(record.value().someOptionalBuffer.value(), expectedBuff);
    }
}
