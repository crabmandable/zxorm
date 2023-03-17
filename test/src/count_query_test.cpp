#include <gtest/gtest.h>
#include <filesystem>
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

class CountQueryTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto created_conn = connection_t::create("test.db", 0, 0, &logger);
        my_conn = std::make_shared<connection_t>(std::move(created_conn));

        my_conn->create_tables(true);
    }

    std::shared_ptr<connection_t> my_conn;

    void TearDown() override {
        my_conn = nullptr;
        std::filesystem::rename("test.db", "test.db.old");
    }
};

TEST_F(CountQueryTest, CountColumnUsingSelectAndFrom) {

    std::vector<Object1> objs;
    objs.resize(10);

    my_conn->insert_many_records(objs);

    auto query = my_conn->select_query<
        Select<Count<table1::field_t<"id">>>,
        From<table1>
    >().one();

    auto result = query.exec();

    ASSERT_EQ(10, result);
}

TEST_F(CountQueryTest, CountDistinctColumnUsingSelectAndFrom) {
    std::vector<Object1> objs = {
        { .text = "hello1" },
        { .text = "hello2" },
        { .text = "hello3" },
        { .text = "hello4" },
        { .text = "hello5" },
        { .text = "hello1" },
        { .text = "hello2" },
        { .text = "hello3" },
        { .text = "hello4" },
        { .text = "hello5" },
    };

    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<
        Select<CountDistinct<table1::field_t<"text">>>,
        From<table1>
    >().one().exec();

    ASSERT_EQ(5, result);
}

TEST_F(CountQueryTest, CountFieldWithoutFrom) {
    std::vector<Object1> objs;
    objs.resize(10);

    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<Count<table1::field_t<"id">>>().one().exec();
    ASSERT_EQ(10, result);
}
TEST_F(CountQueryTest, CountUsingTable) {
    std::vector<Object1> objs;
    objs.resize(10);

    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<Count<table1>>().one().exec();

    ASSERT_EQ(10, result);
}

TEST_F(CountQueryTest, CountAllFrom) {
    std::vector<Object1> objs;
    objs.resize(10);

    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<CountAll, From<table1>>().one().exec();

    ASSERT_EQ(10, result);
}

TEST_F(CountQueryTest, CountAndObjectWithinSelect) {
    std::vector<Object1> objs;
    objs.resize(10);

    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<Select<
        Count<table1::field_t<"id">>, table1
    >>().one().exec();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(10, std::get<0>(result.value()));
    ASSERT_EQ(1, std::get<1>(result.value()).id);
}

TEST_F(CountQueryTest, CountWithGroupBy) {
    for (int i = 0; i < 10; i++) {
        Object1 obj = { .text = i % 2 ? "hello" : "goodbye" };
        my_conn->insert_record(obj);
    }

    auto results = my_conn->select_query<
        Select<CountAll, table1::field_t<"text">>,
        From<table1>
    >().group_by<table1::field_t<"text">>().order_by<table1::field_t<"text">>(order_t::DESC).many().exec();

    auto rows = results.to_vector();
    ASSERT_EQ(rows.size(), 2);

    auto row1 = rows[0];
    size_t count = std::get<0>(row1);
    ASSERT_EQ(count, 5);
    std::string text = std::get<1>(row1);
    ASSERT_EQ(text, "hello");

    auto row2 = rows[1];
    count = std::get<0>(row2);
    ASSERT_EQ(count, 5);
    text = std::get<1>(row2);
    ASSERT_EQ(text, "goodbye");
}

TEST_F(CountQueryTest, CountDistinct) {
    Object1 obj = { .text = "hello" };
    my_conn->insert_record(obj);

    obj = { .text = "goodbye" };
    my_conn->insert_record(obj);

    for (int i = 0; i < 10; i++) {
        Object2 obj = { .obj1_id = i % 2 ? 1 : 2 };
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<CountDistinct<table2::field_t<"obj1_id">>>().one().exec();
    ASSERT_EQ(2, result);
}

TEST_F(CountQueryTest, CountWithJoin) {
    Object1 obj = { .text = "hello" };
    my_conn->insert_record(obj);

    obj = { .text = "goodbye" };
    my_conn->insert_record(obj);

    for (int i = 0; i < 10; i++) {
        Object2 obj = { .obj1_id = i % 2 ? 1 : 2 };
        my_conn->insert_record(obj);
    }

    auto results = my_conn->select_query<
        Select<Count<Object2>, table1::field_t<"text">>,
        From<Object1>,
        Join<Object2>
    >().group_by<table1>().many().exec();

    auto rows = results.to_vector();
    ASSERT_EQ(rows.size(), 2);

    auto row1 = rows[0];
    size_t count = std::get<0>(row1);
    ASSERT_EQ(count, 5);
    std::string text = std::get<1>(row1);
    ASSERT_EQ(text, "hello");

    auto row2 = rows[1];
    count = std::get<0>(row2);
    ASSERT_EQ(count, 5);
    text = std::get<1>(row2);
    ASSERT_EQ(text, "goodbye");
}

TEST_F(CountQueryTest, CountWithWhere) {
    Object1 obj = { .text = "hello" };
    my_conn->insert_record(obj);

    obj = { .text = "goodbye" };
    my_conn->insert_record(obj);
    obj.id = 0;
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<CountAll, From<Object1>>()
        .where_one(table1::field_t<"text">().like("goodbye")).exec();

    ASSERT_EQ(2, result);
}
