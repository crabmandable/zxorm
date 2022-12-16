#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object1 {
    int id = 0;
};

struct Object2 {
    int id = 0;
    int obj1_id = 0;
};

struct Object3 {
    int id = 0;
    int obj1_id = 0;
    int obj2_id = 0;
    std::string more;
    std::string data;
};

using table1 = Table<"obj1", Object1, Column<"id", &Object1::id, PrimaryKey<>>>;

using table2 = Table<"obj2", Object2,
    Column<"id", &Object2::id, PrimaryKey<>>,
    Column<"obj1_id", &Object2::obj1_id, ForeignKey<Reference<"obj1", "id">>>
    >;

using table3 = Table<"obj3", Object3,
    Column<"id", &Object3::id, PrimaryKey<>>,
    Column<"obj1_id", &Object3::obj1_id, ForeignKey<Reference<"obj1", "id">>>,
    Column<"obj2_id", &Object3::obj2_id, ForeignKey<Reference<"obj2", "id">>>
        >;

using connection_t = Connection<table1, table2, table3>;

class ForeignKeysTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto created_conn = connection_t::create("test.db", 0, 0, &logger);

        ASSERT_TRUE(created_conn);

        my_conn = std::make_shared<connection_t>(created_conn.value());

        auto err = my_conn->create_tables(true);
        ASSERT_FALSE(err);
    }

    std::shared_ptr<connection_t> my_conn;

    void TearDown() override {
        my_conn = nullptr;
        std::filesystem::remove("test.db");
    }
};

TEST_F(ForeignKeysTest, PrintForiegnKeys) {
    table3::print_foriegn_keys();
}

TEST_F(ForeignKeysTest, PrintOneForiegnKey) {
    table2::print_foriegn_keys();
}

//TODO add a table that has multiple fk constraints for a single column
