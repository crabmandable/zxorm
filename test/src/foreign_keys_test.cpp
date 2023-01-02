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
};

struct Object4 {
    int id = 0;
    int both = 0;
};

using table1 = Table<"obj1", Object1, Column<"id", &Object1::id, PrimaryKey<>>>;

using table2 = Table<"obj2", Object2,
    Column<"id", &Object2::id, PrimaryKey<>>,
    Column<"obj1_id", &Object2::obj1_id, ForeignKey<"obj1", "id">>
    >;

using table3 = Table<"obj3", Object3,
    Column<"id", &Object3::id, PrimaryKey<>>,
    Column<"obj1_id", &Object3::obj1_id, ForeignKey<"obj1", "id">>,
    Column<"obj2_id", &Object3::obj2_id, ForeignKey<"obj2", "id">>
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

TEST_F(ForeignKeysTest, NoForiegnKeys) {
    ASSERT_EQ(0, std::tuple_size<table1::foreign_columns_t>());
}

TEST_F(ForeignKeysTest, OneForiegnKey) {
    ASSERT_EQ(1, std::tuple_size<table2::foreign_columns_t>());
    using fk = std::remove_reference_t<decltype(std::get<0>(table2::foreign_columns_t{}))>;
    ASSERT_STREQ(fk::name.value, "obj1_id");
}

TEST_F(ForeignKeysTest, ForiegnKeys) {
    ASSERT_EQ(2, std::tuple_size<table3::foreign_columns_t>());
    using fk1 = std::remove_reference_t<decltype(std::get<0>(table3::foreign_columns_t{}))>;
    ASSERT_STREQ(fk1::name.value, "obj1_id");
    using fk2 = std::remove_reference_t<decltype(std::get<1>(table3::foreign_columns_t{}))>;
    ASSERT_STREQ(fk2::name.value, "obj2_id");
}

TEST_F(ForeignKeysTest, FindForeignColumn) {
    using column_t = typename table3::foreign_column<"obj1">;
    ASSERT_STREQ("obj1_id", column_t::name.value);
}

TEST_F(ForeignKeysTest, JoinQueryReturningNothing) {
    auto result = my_conn->select<Object1>().join<table1::field<"id">, table2::field<"obj1_id">>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 0);

    // should be able to swap the params around and get the same query
    result = my_conn->select<Object1>().join<table2::field<"obj1_id">, table1::field<"id">>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    iter = result.value();
    vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 0);
}

TEST_F(ForeignKeysTest, JoinQueryReturningSomething) {
    std::vector<Object1> obj1s;
    obj1s.resize(10);
    auto err = my_conn->insert_many_records(obj1s);
    ASSERT_FALSE(err);

    std::vector<Object2> obj2s;
    for (int i = 1; i <= 3; i++) {
        obj2s.push_back({.obj1_id = i});
    }
    err = my_conn->insert_many_records(obj2s);
    ASSERT_FALSE(err);

    auto result = my_conn->select<Object1>().join<table1::field<"id">, table2::field<"obj1_id">>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);

    // should be able to swap the params around and get the same query
    result = my_conn->select<Object1>().join<table2::field<"obj1_id">, table1::field<"id">>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    iter = result.value();
    vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(ForeignKeysTest, JoinWithWhere) {
    std::vector<Object1> obj1s;
    obj1s.resize(10);
    auto err = my_conn->insert_many_records(obj1s);
    ASSERT_FALSE(err);

    std::vector<Object2> obj2s;
    for (int i = 1; i <= 3; i++) {
        obj2s.push_back({.obj1_id = i});
    }
    err = my_conn->insert_many_records(obj2s);
    ASSERT_FALSE(err);

    auto result = my_conn->select<Object1>()
        .where(table2::field<"id"> == 1)
        .join<table1::field<"id">, table2::field<"obj1_id">>()
        .one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(ForeignKeysTest, JoinUsingFKConstraintReturnNothing) {
    auto result = my_conn->select<Object2>().join<"obj1">().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 0);
}

TEST_F(ForeignKeysTest, JoinUsingFKConstraintReturnSomething) {
    std::vector<Object1> obj1s;
    obj1s.resize(10);
    auto err = my_conn->insert_many_records(obj1s);
    ASSERT_FALSE(err);

    std::vector<Object2> obj2s;
    for (int i = 1; i <= 3; i++) {
        obj2s.push_back({.obj1_id = i});
    }
    err = my_conn->insert_many_records(obj2s);
    ASSERT_FALSE(err);

    auto result = my_conn->select<Object2>().join<"obj1">().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(ForeignKeysTest, GetATupleUsingJoin) {
    Object1 obj1;
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2_1 = {.obj1_id = obj1.id};
    Object2 obj2_2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2_1);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_2);
    ASSERT_FALSE(err);

    auto result = my_conn->select<Object2, Object1>().join<"obj1">().where(table2::field<"id"> == obj2_2.id).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object2 obj2res = std::get<0>(result.value());
    ASSERT_EQ(obj2res.id, obj2_2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object1 obj1res = std::get<1>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
}
