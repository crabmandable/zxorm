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
    bool someBool = false;
    std::optional<float> someOptional;
    std::optional<std::vector<char>> someOptionalBuffer;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::someText, Unique<conflict_t::replace>>,
    Column<"float", &Object::someFloat>,
    Column<"bool", &Object::someBool>,
    Column<"someId", &Object::someId >,
    Column<"someOptional", &Object::someOptional>,
    Column<"someOptionaBuffer", &Object::someOptionalBuffer>
        >;

using MyConnection = Connection<table_t>;

class QueryTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto createdConn = MyConnection::create("test.db", 0, 0, &logger);
        ASSERT_TRUE(createdConn);

        myConn = std::make_shared<MyConnection>(createdConn.value());

        auto createTablesErr = myConn->createTables(true);
        ASSERT_FALSE(createTablesErr);
    }

    std::shared_ptr<MyConnection> myConn;

    void TearDown() override {
        myConn = nullptr;
        std::filesystem::rename("test.db", "test.db.old");
    }
};

TEST_F(QueryTest, FindNothing)
{
    auto result = myConn->findRecord<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        if (record.has_value()) {
            std::cout << record.value().id << std::endl;
            std::cout << record.value().someId << std::endl;
        }
        ASSERT_FALSE(record);
    }
}

TEST_F(QueryTest, InsertSomething)
{
    Object obj;

    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;
    obj.someBool = true;

    auto err = myConn->insertRecord(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }
    ASSERT_FALSE(err);
}

TEST_F(QueryTest, FindSomething)
{
    Object obj;
    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;
    obj.someBool = true;

    auto err = myConn->insertRecord(obj);
    ASSERT_FALSE(err);

    auto result = myConn->findRecord<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().someText, "Some text");
        ASSERT_FLOAT_EQ(record.value().someFloat, 3.14);
        ASSERT_TRUE(record.value().someBool);
        ASSERT_EQ(record.value().someId, 42);
        ASSERT_FALSE(record.value().someOptional.has_value());
        ASSERT_FALSE(record.value().someOptionalBuffer.has_value());
    }
}

TEST_F(QueryTest, FindSomethingWithOptionalsFilled)
{
    Object obj;
    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;
    obj.someOptional = 42.333;
    obj.someOptionalBuffer = {'y', 'o'};

    auto err = myConn->insertRecord(obj);
    ASSERT_FALSE(err);

    auto result = myConn->findRecord<Object>(1);
    if (result.is_error()) {
        std::cout << std::string(result.error()) << std::endl;
        ASSERT_FALSE(true);
    } else {
        auto record = result.value();
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().someText, "Some text");
        ASSERT_FLOAT_EQ(record.value().someFloat, 3.14);
        ASSERT_FALSE(record.value().someBool);
        ASSERT_EQ(record.value().someId, 42);
        ASSERT_TRUE(record.value().someOptional.has_value());
        ASSERT_FLOAT_EQ(record.value().someOptional.value(), 42.333);
        auto expectedBuff = std::vector<char> {'y', 'o'};
        ASSERT_EQ(record.value().someOptionalBuffer.value(), expectedBuff);
    }
}

TEST_F(QueryTest, DeleteSomething)
{
    Object obj;
    obj.someText = "Some text";
    obj.someFloat = 3.14;
    obj.someId = 42;
    obj.someOptional = 42.333;
    obj.someOptionalBuffer = {'y', 'o'};

    auto err = myConn->insertRecord(obj);
    ASSERT_FALSE(err);
    err = myConn->deleteRecord<Object>(1);
    ASSERT_FALSE(err);
    auto result = myConn->findRecord<Object>(1);
    ASSERT_TRUE(result);
    ASSERT_FALSE(result.value());
}
