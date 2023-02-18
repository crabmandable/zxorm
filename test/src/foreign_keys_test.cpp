#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object1 {
    int id = 0;
    std::string text;
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

using table1 = Table<"obj1", Object1,
      Column<"id", &Object1::id, PrimaryKey<>>,
      Column<"text", &Object1::text>>;

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
        std::filesystem::rename("test.db", "test.db.old");
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
    auto result = my_conn->select_query<Object1,
         JoinOn<table1::field_t<"id">, table2::field_t<"obj1_id">>
    >().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 0);

    // should be able to swap the params around and get the same query
    result = my_conn->select_query<Object1, JoinOn<table2::field_t<"obj1_id">, table1::field_t<"id">>>().many();

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

    auto result = my_conn->select_query<Object1, JoinOn<table1::field_t<"id">, table2::field_t<"obj1_id">>>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);

    // should be able to swap the params around and get the same query
    result = my_conn->select_query<Object1, JoinOn<table2::field_t<"obj1_id">, table1::field_t<"id">>>().many();

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

    auto result = my_conn->select_query<Object1, JoinOn<table1::field_t<"id">, table2::field_t<"obj1_id">>>()
        .where(table2::field_t<"id">() == 1)
        .one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(ForeignKeysTest, JoinUsingFKConstraintReturnNothing) {

    auto result = my_conn->select_query<Object2, Join<Object1>>().many();

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

    auto result = my_conn->select_query<Object2, Join<Object1>>().many();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 3);
}

TEST_F(ForeignKeysTest, GetATupleUsingJoin) {
    Object1 obj1 = {.text = "sup"};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2_1 = {.obj1_id = obj1.id};
    Object2 obj2_2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2_1);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_2);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<Select<Object2, Object1>, Join<Object1>>().where(table2::field_t<"id">() == obj2_2.id).one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object2 obj2res = std::get<0>(result.value());
    ASSERT_EQ(obj2res.id, obj2_2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object1 obj1res = std::get<1>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), "sup");
}

TEST_F(ForeignKeysTest, GetManyTupleUsingJoin) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2_1 = {.obj1_id = obj1.id};
    Object2 obj2_2 = {.obj1_id = obj1.id};
    Object2 obj2_3 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2_1);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_2);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_3);
    ASSERT_FALSE(err);

    auto results = my_conn->select_query<Select<Object2, Object1>, Join<Object1>>().many();

    if (results.is_error()) std::cout << results.error() << std::endl;
    ASSERT_FALSE(results.is_error());

    size_t i = 1;
    for (const auto& row: results.value()) {
        if (row.is_error()) std::cout << row.error() << std::endl;
        ASSERT_FALSE(row.is_error());
        Object2 obj2res = std::get<0>(row.value());
        ASSERT_EQ(obj2res.id, i++);
        ASSERT_EQ(obj2res.obj1_id, obj1.id);

        Object1 obj1res = std::get<1>(row.value());
        ASSERT_EQ(obj1res.id, obj1.id);
        ASSERT_STREQ(obj1res.text.c_str(), test_text);
    }
    ASSERT_EQ(i, 4);
}

TEST_F(ForeignKeysTest, MultipleJoins) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<Object3, Object2, Object1>,
        Join<Object2>,
        Join<Object1>
    >().one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object3 obj3res = std::get<0>(result.value());
    ASSERT_EQ(obj3res.id, obj3.id);
    ASSERT_EQ(obj3res.obj2_id, obj2.id);
    ASSERT_EQ(obj3res.obj1_id, obj1.id);

    Object2 obj2res = std::get<1>(result.value());
    ASSERT_EQ(obj2res.id, obj2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object1 obj1res = std::get<2>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), test_text);
}

TEST_F(ForeignKeysTest, MultipleJoinsUsingTableClasses) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<table3, table2, table1>,
        From<table3>,
        Join<table2>,
        Join<table1>
    >().one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object3 obj3res = std::get<0>(result.value());
    ASSERT_EQ(obj3res.id, obj3.id);
    ASSERT_EQ(obj3res.obj2_id, obj2.id);
    ASSERT_EQ(obj3res.obj1_id, obj1.id);

    Object2 obj2res = std::get<1>(result.value());
    ASSERT_EQ(obj2res.id, obj2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object1 obj1res = std::get<2>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), test_text);
}

TEST_F(ForeignKeysTest, MultipleJoinsUsingTableClassesOppositeOrder) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<table3, table2, table1>,
        From<table1>,
        Join<table2>,
        Join<table3>
    >().one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object3 obj3res = std::get<0>(result.value());
    ASSERT_EQ(obj3res.id, obj3.id);
    ASSERT_EQ(obj3res.obj2_id, obj2.id);
    ASSERT_EQ(obj3res.obj1_id, obj1.id);

    Object2 obj2res = std::get<1>(result.value());
    ASSERT_EQ(obj2res.id, obj2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object1 obj1res = std::get<2>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), test_text);
}

TEST_F(ForeignKeysTest, JoinWithFrom) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<Object2, Object3, Object1>,
        From<Object3>,
        Join<Object2>,
        JoinOn<table2::field_t<"obj1_id">, table1::field_t<"id">>
    >().one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object2 obj2res = std::get<0>(result.value());
    ASSERT_EQ(obj2res.id, obj2.id);
    ASSERT_EQ(obj2res.obj1_id, obj1.id);

    Object3 obj3res = std::get<1>(result.value());
    ASSERT_EQ(obj3res.id, obj3.id);
    ASSERT_EQ(obj3res.obj2_id, obj2.id);
    ASSERT_EQ(obj3res.obj1_id, obj1.id);

    Object1 obj1res = std::get<2>(result.value());
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), test_text);
}

TEST_F(ForeignKeysTest, SelectDifferentRecordUsingJoin) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto result = my_conn->select_query<
        Select<Object1>,
        From<Object3>,
        JoinOn<table3::field_t<"obj1_id">, table1::field_t<"id">>
    >().one();

    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    Object1 obj1res = result.value();
    ASSERT_EQ(obj1res.id, obj1.id);
    ASSERT_STREQ(obj1res.text.c_str(), test_text);
}

TEST_F(ForeignKeysTest, LeftJoin) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2_1 = {.obj1_id = obj1.id};
    Object2 obj2_2 = {.obj1_id = obj1.id};
    Object2 obj2_3 = {.obj1_id = obj1.id + 10};
    err = my_conn->insert_record(obj2_1);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_2);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_3);
    ASSERT_FALSE(err);

    auto results = my_conn->select_query<Select<Object2, Object1>, Join<Object1, join_type_t::LEFT_OUTER>>().many();

    if (results.is_error()) std::cout << results.error() << std::endl;
    ASSERT_FALSE(results.is_error());

    auto data = results.value().to_vector();
    ASSERT_FALSE(data.is_error());
    auto rows = std::move(data.value());
    ASSERT_EQ(rows.size(), 3);

    ASSERT_TRUE(std::get<1>(rows[0]).has_value());
    ASSERT_EQ(std::get<1>(rows[0]).value().id, obj1.id);

    ASSERT_TRUE(std::get<1>(rows[1]).has_value());
    ASSERT_EQ(std::get<1>(rows[1]).value().id, obj1.id);

    ASSERT_FALSE(std::get<1>(rows[2]).has_value());
}

TEST_F(ForeignKeysTest, RightJoin) {
    auto test_text = "hello there";
    Object1 obj1 = {.text = test_text};
    auto err = my_conn->insert_record(obj1);
    ASSERT_FALSE(err);

    Object2 obj2_1 = {.obj1_id = obj1.id};
    Object2 obj2_2 = {.obj1_id = obj1.id};
    Object2 obj2_3 = {.obj1_id = obj1.id + 10};
    err = my_conn->insert_record(obj2_1);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_2);
    ASSERT_FALSE(err);
    err = my_conn->insert_record(obj2_3);
    ASSERT_FALSE(err);

    auto results = my_conn->select_query<Select<Object1, Object2>,
         JoinOn<table1::field_t<"id">, table2::field_t<"obj1_id">, join_type_t::RIGHT_OUTER>>().many();

    if (results.is_error()) std::cout << results.error() << std::endl;
    ASSERT_FALSE(results.is_error());

    auto data = results.value().to_vector();
    ASSERT_FALSE(data.is_error());
    auto rows = std::move(data.value());
    ASSERT_EQ(rows.size(), 3);

    ASSERT_TRUE(std::get<0>(rows[0]).has_value());
    ASSERT_EQ(std::get<0>(rows[0]).value().id, obj1.id);

    ASSERT_TRUE(std::get<0>(rows[1]).has_value());
    ASSERT_EQ(std::get<0>(rows[1]).value().id, obj1.id);

    ASSERT_FALSE(std::get<0>(rows[2]).has_value());
}

TEST_F(ForeignKeysTest, NestedOuter) {
    auto test_text_1 = "hello there";
    auto test_text_2 = "howdy";
    Object1 obj1_1 = {.text = test_text_1};
    auto err = my_conn->insert_record(obj1_1);
    ASSERT_FALSE(err);
    Object1 obj1_2 = {.text = test_text_2};
    err = my_conn->insert_record(obj1_2);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1_1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1_1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto results = my_conn->select_query<
        Select<Object3, Object2, Object1>,
        Join<Object2>,
        JoinOn<table2::field_t<"obj1_id">, table1::field_t<"id">, join_type_t::FULL_OUTER>
    >().many();

    if (results.is_error()) std::cout << results.error() << std::endl;
    ASSERT_FALSE(results.is_error());

    auto data = results.value().to_vector();
    ASSERT_FALSE(data.is_error());
    auto rows = std::move(data.value());
    ASSERT_EQ(rows.size(), 2);

    auto row1 = rows[0];

    auto obj3res = std::get<0>(row1);
    ASSERT_TRUE(obj3res.has_value());
    ASSERT_EQ(obj3res.value().id, obj3.id);
    ASSERT_EQ(obj3res.value().obj2_id, obj2.id);
    ASSERT_EQ(obj3res.value().obj1_id, obj1_1.id);

    auto obj2res = std::get<1>(row1);
    ASSERT_TRUE(obj2res.has_value());
    ASSERT_EQ(obj2res.value().id, obj2.id);
    ASSERT_EQ(obj2res.value().obj1_id, obj1_1.id);

    auto obj1res = std::get<2>(row1);
    ASSERT_TRUE(obj1res.has_value());
    ASSERT_EQ(obj1res.value().id, obj1_1.id);
    ASSERT_STREQ(obj1res.value().text.c_str(), test_text_1);

    auto row2 = rows[1];

    obj3res = std::get<0>(row2);
    ASSERT_FALSE(obj3res.has_value());
    obj2res = std::get<1>(row2);
    ASSERT_FALSE(obj2res.has_value());
    obj1res = std::get<2>(row2);
    ASSERT_TRUE(obj1res.has_value());
    ASSERT_EQ(obj1res.value().id, obj1_2.id);
    ASSERT_STREQ(obj1res.value().text.c_str(), test_text_2);
}

TEST_F(ForeignKeysTest, SelectAColumnWithJoins) {
    Object1 obj1_1;
    auto err = my_conn->insert_record(obj1_1);
    ASSERT_FALSE(err);
    Object1 obj1_2;
    err = my_conn->insert_record(obj1_2);
    ASSERT_FALSE(err);

    Object2 obj2 = {.obj1_id = obj1_1.id};
    err = my_conn->insert_record(obj2);
    ASSERT_FALSE(err);

    Object3 obj3 = {.obj1_id = obj1_1.id, .obj2_id = obj2.id};
    err = my_conn->insert_record(obj3);
    ASSERT_FALSE(err);

    auto results = my_conn->select_query<
        Select<table3::field_t<"id">>,
        Join<Object2>,
        JoinOn<table2::field_t<"obj1_id">, table1::field_t<"id">, join_type_t::FULL_OUTER>
    >().many();

    if (results.is_error()) std::cout << results.error() << std::endl;
    ASSERT_FALSE(results.is_error());

    auto data = results.value().to_vector();
    ASSERT_FALSE(data.is_error());
    auto rows = std::move(data.value());
    ASSERT_EQ(rows.size(), 2);

    ASSERT_TRUE(rows[0].has_value());
    ASSERT_EQ(rows[0].value(), obj3.id);

    ASSERT_FALSE(rows[1].has_value());
}
