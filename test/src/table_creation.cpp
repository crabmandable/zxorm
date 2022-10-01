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

using table_one_t = Table<"one", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::some_text, Unique<conflict_t::replace>>,
    Column<"float", &Object::some_float>,
    Column<"bool", &Object::some_bool>,
    Column<"some_id", &Object::some_id >,
    Column<"some_optional", &Object::some_optional>,
    Column<"someOptionaBuffer", &Object::some_optional_buffer>
        >;

using table_two_t = Table<"two", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::some_text, Unique<conflict_t::replace>>,
    Column<"float", &Object::some_float>,
    Column<"bool", &Object::some_bool>,
    Column<"some_id", &Object::some_id >,
    Column<"some_optional", &Object::some_optional>,
    Column<"someOptionaBuffer", &Object::some_optional_buffer>
        >;

using table_three_t = Table<"three", Object,
    Column<"id", &Object::id, PrimaryKey<conflict_t::abort>>,
    Column<"text", &Object::some_text, Unique<conflict_t::replace>>,
    Column<"float", &Object::some_float>,
    Column<"bool", &Object::some_bool>,
    Column<"some_id", &Object::some_id >,
    Column<"some_optional", &Object::some_optional>,
    Column<"someOptionaBuffer", &Object::some_optional_buffer>
        >;


class TableCreationTest : public ::testing::Test {
    protected:
    void SetUp() override {
    }


    void TearDown() override {
        std::filesystem::rename("test.db", "test.db.old");
    }
};

template<typename... T>
auto make_connection() -> std::shared_ptr<Connection<T...>> {
    using connection_t = Connection<T...>;
    auto created_conn = connection_t::create("test.db", 0, 0, &logger);
    if (!created_conn) {
        return nullptr;
    }
    return std::make_shared<connection_t>(created_conn.value());
}

TEST_F(TableCreationTest, CreateTables) {
    auto my_conn = make_connection<table_one_t>();
    auto err = my_conn->create_tables(false);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);
    auto count = my_conn->count_tables();
    ASSERT_FALSE(count.is_error());
    ASSERT_EQ(count.value(), 1);
}

TEST_F(TableCreationTest, CreateIfExistsTables) {
    auto my_conn = make_connection<table_one_t>();
    auto err = my_conn->create_tables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);

    auto count = my_conn->count_tables();
    ASSERT_FALSE(count.is_error());
    ASSERT_EQ(count.value(), 1);

    err = my_conn->create_tables(true);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);

    count = my_conn->count_tables();
    ASSERT_FALSE(count.is_error());
    ASSERT_EQ(count.value(), 1);
}

TEST_F(TableCreationTest, CreateManyTables) {
    auto my_conn = make_connection<table_one_t, table_two_t, table_three_t>();
    auto err = my_conn->create_tables(false);
    if (err) std::cout << std::string(err.value()) << std::endl;
    ASSERT_FALSE(err);

    auto count = my_conn->count_tables();
    ASSERT_FALSE(count.is_error());
    ASSERT_EQ(count.value(), 3);
}

TEST_F(TableCreationTest, AllOrNothingTransaction) {
    auto my_conn = make_connection<table_one_t, table_one_t, table_three_t>();
    auto err = my_conn->create_tables(false);
    ASSERT_TRUE(err.has_value());

    auto count = my_conn->count_tables();
    ASSERT_FALSE(count.is_error());
    ASSERT_EQ(count.value(), 0);
}
