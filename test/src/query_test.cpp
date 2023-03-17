#include <gtest/gtest.h>
#include <filesystem>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int id = 0;
    int some_id = 13;
    std::string some_text = "heelllo";
    float some_float = 11.0;
    bool some_bool = false;
    std::optional<float> some_optional;
    std::optional<std::vector<char>> some_optional_buffer;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::id, PrimaryKey<>>,
    Column<"text", &Object::some_text>,
    Column<"float", &Object::some_float>,
    Column<"bool", &Object::some_bool>,
    Column<"some_id", &Object::some_id >,
    Column<"some_optional", &Object::some_optional>,
    Column<"some_optional_buffer", &Object::some_optional_buffer>
        >;

struct OtherObj {
    std::string some_text;
};

using pk_is_text_t = Table<"test2", OtherObj,
    Column<"text", &OtherObj::some_text, PrimaryKey<conflict_t::abort>>>;

using connection_t = Connection<table_t, pk_is_text_t>;

class QueryTest : public ::testing::Test {
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

TEST_F(QueryTest, FindNothing)
{
    std::optional<Object> result = my_conn->find_record<Object>(1);
    ASSERT_FALSE(result);
}

TEST_F(QueryTest, InsertSomething)
{
    Object obj;

    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    my_conn->insert_record(obj);
    ASSERT_EQ(obj.id, 1);
}

TEST_F(QueryTest, InsertObjWithoutRowId)
{
    OtherObj obj = { .some_text = "Some text" };
    ASSERT_NO_THROW(my_conn->insert_record(obj));
}

TEST_F(QueryTest, FindSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    my_conn->insert_record(obj);

    std::optional<Object> record = my_conn->find_record<Object>(1);
    ASSERT_TRUE(record);
    ASSERT_EQ(record.value().id, 1);
    ASSERT_EQ(record.value().some_text, "Some text");
    ASSERT_FLOAT_EQ(record.value().some_float, 3.14);
    ASSERT_TRUE(record.value().some_bool);
    ASSERT_EQ(record.value().some_id, 42);
    ASSERT_FALSE(record.value().some_optional.has_value());
    ASSERT_FALSE(record.value().some_optional_buffer.has_value());
}

TEST_F(QueryTest, FindManyTimes)
{
    std::vector<std::string> text = {
        "hello",
        "there",
        "this",
        "is",
        "text",
    };

    for (size_t i = 0; i < text.size(); i++) {
        Object obj;
        obj.some_text = text[i];
        my_conn->insert_record(obj);
    }

    for (int id = 1; id <= 5; id++) {
        std::optional<Object> record = my_conn->find_record<Object>(id);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, id);
        ASSERT_EQ(record.value().some_text, text[id - 1]);
    }
}

TEST_F(QueryTest, FindSomethingWithOptionalsFilled)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    my_conn->insert_record(obj);

    auto result = my_conn->find_record<Object>(1);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
    ASSERT_EQ(result.value().some_text, "Some text");
    ASSERT_FLOAT_EQ(result.value().some_float, 3.14);
    ASSERT_FALSE(result.value().some_bool);
    ASSERT_EQ(result.value().some_id, 42);
    ASSERT_TRUE(result.value().some_optional.has_value());
    ASSERT_FLOAT_EQ(result.value().some_optional.value(), 42.333);
    auto expectedBuff = std::vector<char> {'y', 'o'};
    ASSERT_EQ(result.value().some_optional_buffer.value(), expectedBuff);
}

TEST_F(QueryTest, DeleteSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    my_conn->insert_record(obj);
    my_conn->delete_record<Object>(1);

    auto result = my_conn->find_record<Object>(1);
    ASSERT_FALSE(result.has_value());
}

TEST_F(QueryTest, WhereEq)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    my_conn->insert_record(obj);

    auto results = my_conn->select_query<Object>().where_many(table_t::field_t<"some_id">() == 42).exec();

    int i = 0;
    for (const auto& record: results) {
        // we should only see one record
        ASSERT_TRUE(i++ < 1);

        ASSERT_EQ(obj.id, record.id);
        ASSERT_EQ(1, record.id);
        ASSERT_EQ(obj.some_text, record.some_text);
        ASSERT_EQ(obj.some_id, record.some_id);
        ASSERT_EQ(obj.some_float, record.some_float);
        ASSERT_EQ(obj.some_optional, record.some_optional);
        ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
    }

    auto vec = results.to_vector();
    ASSERT_EQ(vec.size(), 1);

    auto& record = vec[0];
    ASSERT_EQ(obj.id, record.id);
    ASSERT_EQ(1, record.id);
    ASSERT_EQ(obj.some_text, record.some_text);
    ASSERT_EQ(obj.some_id, record.some_id);
    ASSERT_EQ(obj.some_float, record.some_float);
    ASSERT_EQ(obj.some_optional, record.some_optional);
    ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
}

TEST_F(QueryTest, WhereFindNothing)
{
    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"some_id">() == 42).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 0);
}

TEST_F(QueryTest, WhereFindMany)
{
    Object obj;
    obj.some_id = 42;

    for (size_t i = 0; i < 4; i++) {
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"some_id">() == 42).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 4);
}

TEST_F(QueryTest, WhereEqOrEq)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(
            table_t::field_t<"some_id">() == 0 ||
            table_t::field_t<"some_id">() == 1 ||
            table_t::field_t<"some_id">() == 2
        ).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 3);
}

TEST_F(QueryTest, WhereNe)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"some_id">() != 0).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 3);
}

TEST_F(QueryTest, WhereNeAndNe)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(
            table_t::field_t<"some_id">() != 0 &&
            table_t::field_t<"id">() != 2).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, WhereLt)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"id">() < 2).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 1);
}

TEST_F(QueryTest, WhereLte)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"id">() <= 2).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, WhereGt)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"id">() > 2).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, WhereGte)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_id = i;
        my_conn->insert_record(obj);
    }

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"id">() >= 2).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 3);
}

TEST_F(QueryTest, WhereLike)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"text">().like(std::string("hello_"))).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 4);
}

TEST_F(QueryTest, WhereNotLike)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"text">().not_like("hello_")).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 1);
}

TEST_F(QueryTest, WhereGlob)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"text">().glob("hello*")).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 4);
}

TEST_F(QueryTest, WhereNotGlob)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().where_many(table_t::field_t<"text">().not_glob("hell*")).exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 1);
}

TEST_F(QueryTest, WhereIn)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    auto search = std::vector<std::string>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where_many(table_t::field_t<"text">().in(search)).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, WhereNotIn)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    auto search = std::array<std::string, 2>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where_many(table_t::field_t<"text">().not_in(search)).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, All)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().many().exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 6);
}

TEST_F(QueryTest, SelectFrom)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Select<Object>, From<Object>>().many().exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 6);
}

TEST_F(QueryTest, SelectWithoutFrom)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Select<Object>>().many().exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 6);
}

TEST_F(QueryTest, OrderDesc)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().order_by<table_t::field_t<"id">>(order_t::DESC).many().exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 6);
    ASSERT_EQ(vec[0].id, 6);
    ASSERT_EQ(vec[1].id, 5);
    ASSERT_EQ(vec[2].id, 4);
    ASSERT_EQ(vec[3].id, 3);
    ASSERT_EQ(vec[4].id, 2);
    ASSERT_EQ(vec[5].id, 1);
}

TEST_F(QueryTest, OrderAscOne)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().order_by<table_t::field_t<"id">>(order_t::ASC).one().exec();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(QueryTest, OrderAscLimit)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>()
        .order_by<table_t::field_t<"id">>(order_t::DESC)
        .limit(3)
        .many()
        .exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[0].id, 6);
    ASSERT_EQ(vec[1].id, 5);
    ASSERT_EQ(vec[2].id, 4);
}

TEST_F(QueryTest, LimitWithOffset)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>()
        .limit(3, 3)
        .many()
        .exec();
    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 3);
    ASSERT_EQ(vec[0].id, 4);
    ASSERT_EQ(vec[1].id, 5);
    ASSERT_EQ(vec[2].id, 6);
}

TEST_F(QueryTest, EmptyOne)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<Object>().where_one(table_t::field_t<"id">() > 10).exec();
    ASSERT_FALSE(result.has_value());
}

TEST_F(QueryTest, First)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->first<Object>();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 1);
}

TEST_F(QueryTest, Last)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = std::string("helllo4");
    my_conn->insert_record(obj);
    obj.some_text = std::string("h5");
    my_conn->insert_record(obj);

    auto result = my_conn->last<Object>();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().id, 6);
}

TEST_F(QueryTest, UpdateSomething)
{
    Object obj;

    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    my_conn->insert_record(obj);

    obj.some_text = "Some different text";
    my_conn->update_record(obj);

    auto result = my_conn->find_record<Object>(obj.id);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().some_text, std::string("Some different text"));
}

TEST_F(QueryTest, InsertMany)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    my_conn->insert_many_records(objects);

    auto result = my_conn->select_query<Object>().many().exec();

    auto inserted = result.to_vector();
    ASSERT_EQ(200, inserted.size());
    for (int i = 0; i < 200; i++) {
        ASSERT_EQ(i, inserted[i].some_id);
    }
}

TEST_F(QueryTest, DeleteWhere)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    my_conn->insert_many_records(objects);

    my_conn->delete_query<Object>().where(table_t::field_t<"float">() >= (100.0 * 3.14)).exec();

    auto result = my_conn->select_query<Object>().many().exec();
    auto undeleted = result.to_vector();
    ASSERT_EQ(100, undeleted.size());
}

TEST_F(QueryTest, Truncate)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    my_conn->insert_many_records(objects);

    my_conn->truncate<Object>();

    auto result = my_conn->select_query<Object>().many().exec();
    auto undeleted = result.to_vector();
    ASSERT_EQ(0, undeleted.size());
}

TEST_F(QueryTest, BindStringView) {
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    obj.some_text = "something else";
    my_conn->insert_record(obj);

    std::string_view search = "hello%";

    auto result = my_conn->select_query<Object>()
        .where_many(table_t::field_t<"text">().like(search)).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 4);
}

TEST_F(QueryTest, BindStringViewVector)
{
    Object obj;
    for (size_t i = 0; i < 4; i++) {
        obj.some_text = std::string("hello") + std::to_string(i);
        my_conn->insert_record(obj);
    }

    auto search = std::vector<std::string_view>{"hello1", "hello2"};

    auto result = my_conn->select_query<Object>()
        .where_many(table_t::field_t<"text">().in(search)).exec();

    auto vec = result.to_vector();
    ASSERT_EQ(vec.size(), 2);
}

TEST_F(QueryTest, SelectAColumn)
{
    std::vector<Object> objs;
    objs.resize(4);
    my_conn->insert_many_records(objs);

    auto result = my_conn->select_query<table_t::field_t<"id">>()
        .order_by<table_t::field_t<"id">>(order_t::DESC).one().exec();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), 4);
}

TEST_F(QueryTest, SelectSeveralColumns)
{
    Object obj;
    obj.some_text = "yes";
    my_conn->insert_record(obj);

    auto result = my_conn->select_query<
        Select<table_t::field_t<"id">, table_t::field_t<"text">>
    >().order_by<table_t::field_t<"id">>(order_t::DESC).one().exec();

    ASSERT_TRUE(result.has_value());
    int id = std::get<0>(result.value());
    ASSERT_EQ(1, id);

    std::string text = std::get<1>(result.value());
    ASSERT_EQ(text, "yes");
}

TEST_F(QueryTest, ReuseAQuery)
{
    Object obj;
    obj.some_text = "yes";
    my_conn->insert_record(obj);

    auto query = my_conn->select_query<table_t::field_t<"text">>()
        .order_by<table_t::field_t<"id">>(order_t::DESC).one();

    auto result = query.exec();

    ASSERT_TRUE(result.has_value());
    std::string text = result.value();
    ASSERT_EQ(text, "yes");

    obj.id = 0;
    obj.some_text = "nope";
    my_conn->insert_record(obj);

    result = query.exec();
    ASSERT_TRUE(result.has_value());
    text = result.value();
    ASSERT_EQ(text, "nope");
}

TEST_F(QueryTest, ReuseAQueryWithMultipleBinds)
{
    std::vector<Object> objects;
    for (int i = 0; i < 200; i++) {
        objects.push_back({
            .some_id = i,
            .some_text = "this is some text" + std::to_string(i),
            .some_float = float(3.14 * i),
            .some_bool = true,
            .some_optional = float(i),
            .some_optional_buffer = std::vector<char> {(char)i, 'b'}
        });
    }

    my_conn->insert_many_records(objects);

    auto query = my_conn->select_query<table_t::field_t<"float">>()
        .where_many(table_t::field_t<"id">() == 1|| table_t::field_t<"id">() == 2);

    auto vec = query.exec().to_vector();
    ASSERT_EQ(vec[0], 0);
    ASSERT_EQ(vec[1], 3.14f);

    query.rebind(4, 5);

    vec = query.exec().to_vector();
    ASSERT_EQ(vec[0], 3.14f * 3);
    ASSERT_EQ(vec[1], 3.14f * 4);
}
