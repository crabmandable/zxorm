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

class TableCreationTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto createdConn = MyConnection::create("test.db", 0, 0, &logger);
        if (!createdConn) {
            throw "Unable to open connection";
        }

        myConn = std::make_shared<MyConnection>(createdConn.value());
    }

    std::shared_ptr<MyConnection> myConn;

    void TearDown() override {
        myConn = nullptr;
        std::filesystem::rename("test.db", "test.db.old");
    }
};

TEST_F(TableCreationTest, CreateTables) {
    auto err = myConn->createTables(false);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);
}

TEST_F(TableCreationTest, CreateIfExistsTables) {
    auto err = myConn->createTables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);

    err = myConn->createTables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);
}
