#include <gtest/gtest.h>
#include <filesystem>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int id = 0;
    int some_id = 0;
    std::string some_text;
    float some_float;
    bool some_bool = false;
    std::optional<float> some_optional;
    std::optional<std::vector<char>> some_optional_buffer;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::some_text, Unique<conflict_t::replace>>,
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
        ASSERT_FALSE(created_conn.is_error());

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

TEST_F(QueryTest, FindNothing)
{
    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        if (record.has_value()) {
            std::cout << record.value().id << std::endl;
            std::cout << record.value().some_id << std::endl;
        }
        ASSERT_FALSE(record);
    }
}

TEST_F(QueryTest, InsertSomething)
{
    Object obj;

    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    auto err = my_conn->insert_record(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }
    ASSERT_FALSE(err);
    ASSERT_EQ(obj.id, 1);
}

TEST_F(QueryTest, InsertObjWithoutRowId)
{
    OtherObj obj = { .some_text = "Some text" };

    auto err = my_conn->insert_record(obj);
    if (err) {
        std::cout << std::string(err.value()) << std::endl;
    }

    ASSERT_FALSE(err);
}

TEST_F(QueryTest, FindSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_bool = true;

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        auto err = result.error();
        std::cout << std::string(err) << std::endl;
        ASSERT_FALSE(true);
    } else {
        std::optional<Object> record = std::move(result);
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().some_text, "Some text");
        ASSERT_FLOAT_EQ(record.value().some_float, 3.14);
        ASSERT_TRUE(record.value().some_bool);
        ASSERT_EQ(record.value().some_id, 42);
        ASSERT_FALSE(record.value().some_optional.has_value());
        ASSERT_FALSE(record.value().some_optional_buffer.has_value());
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

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);

    auto result = my_conn->find_record<Object>(1);
    if (result.is_error()) {
        std::cout << std::string(result.error()) << std::endl;
        ASSERT_FALSE(true);
    } else {
        auto record = result.value();
        ASSERT_TRUE(record);
        ASSERT_EQ(record.value().id, 1);
        ASSERT_EQ(record.value().some_text, "Some text");
        ASSERT_FLOAT_EQ(record.value().some_float, 3.14);
        ASSERT_FALSE(record.value().some_bool);
        ASSERT_EQ(record.value().some_id, 42);
        ASSERT_TRUE(record.value().some_optional.has_value());
        ASSERT_FLOAT_EQ(record.value().some_optional.value(), 42.333);
        auto expectedBuff = std::vector<char> {'y', 'o'};
        ASSERT_EQ(record.value().some_optional_buffer.value(), expectedBuff);
    }
}

TEST_F(QueryTest, DeleteSomething)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);
    err = my_conn->delete_record<Object>(1);
    ASSERT_FALSE(err);
    auto result = my_conn->find_record<Object>(1);
    ASSERT_FALSE(result.is_error());
    ASSERT_FALSE(result.value());
}

TEST_F(QueryTest, WhereEq)
{
    Object obj;
    obj.some_text = "Some text";
    obj.some_float = 3.14;
    obj.some_id = 42;
    obj.some_optional = 42.333;
    obj.some_optional_buffer = {'y', 'o'};

    auto err = my_conn->insert_record(obj);
    ASSERT_FALSE(err);
    auto result = my_conn->where<Object>(table_t::field<"some_id"> == 42);
    if (result.is_error()) std::cout << result.error() << std::endl;
    ASSERT_FALSE(result.is_error());

    auto iter = result.value();
    int i = 0;
    for (const auto& result: iter) {
        // we should only see one record
        ASSERT_TRUE(i++ < 1);
        ASSERT_FALSE(result.is_error());
        ASSERT_TRUE(result.value());

        // ew valuevalue
        auto& record = result.value().value();

        ASSERT_EQ(obj.id, record.id);
        ASSERT_EQ(1, record.id);
        ASSERT_EQ(obj.some_text, record.some_text);
        ASSERT_EQ(obj.some_id, record.some_id);
        ASSERT_EQ(obj.some_float, record.some_float);
        ASSERT_EQ(obj.some_optional, record.some_optional);
        ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
    }

    auto vec = iter.to_vector();
    ASSERT_FALSE(vec.is_error());
    ASSERT_EQ(vec.value().size(), 1);

    auto& record = vec.value()[0];
    ASSERT_EQ(obj.id, record.id);
    ASSERT_EQ(1, record.id);
    ASSERT_EQ(obj.some_text, record.some_text);
    ASSERT_EQ(obj.some_id, record.some_id);
    ASSERT_EQ(obj.some_float, record.some_float);
    ASSERT_EQ(obj.some_optional, record.some_optional);
    ASSERT_EQ(obj.some_optional_buffer, record.some_optional_buffer);
}
