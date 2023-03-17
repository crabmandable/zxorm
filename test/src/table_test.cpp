#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include "zxorm/zxorm.hpp"
#include "logger.hpp"

using namespace zxorm;

struct Object {
    int _id = 0;
    std::string _name;
    int _some_id = 0;
    std::string _some_text;
    float _some_float;
    auto get_id() { return _id; }
    void set_id(int id) { _id = id; }
    auto get_name() { return _name; }
    void set_name(std::string name) { _name = name; }
};

struct Object2 {
    int _id = 0;
    std::string _name;
    int _some_id = 0;
    std::string _some_text;
    float _some_float;
    auto get_id() { return _id; }
    void set_id(int id) { _id = id; }
    auto get_name() { return _name; }
    void set_name(std::string name) { _name = name; }
};

struct Object3 {
    int _id = 0;
    std::string _name;
    int _some_id = 0;
    std::string _some_text;
    float _some_float;
    auto get_id() { return _id; }
    void set_id(int id) { _id = id; }
    auto get_name() { return _name; }
    void set_name(std::string name) { _name = name; }
};

struct Object4 {
    int id = 0;
    std::optional<std::string> opt_text;
    std::string text;
    std::string more_text;
    std::string not_null_text;
};

using table_t = Table<"test", Object,
    Column<"id", &Object::_id>,
    Column<"name", &Object::_name>
        > ;

using tablepriv_t = Table<"test_private", Object2,
    ColumnPrivate<"id",&Object2::get_id, &Object2::set_id>,
    ColumnPrivate<"name", &Object2::get_name, &Object2::set_name>
        > ;

using table_with_column_constraints_t = Table<"test_constraints", Object3,
    Column<"id", &Object3::_id, PrimaryKey<conflict_t::abort>>,
    Column<"name", &Object3::_name, NotNull<>, Unique<>>,
    Column<"text", &Object3::_some_text, Unique<conflict_t::replace>>,
    Column<"float", &Object3::_some_float>,
    Column<"someId", &Object3::_some_id, ForeignKey<"test", "id", action_t::cascade, action_t::restrict>>
        >;

using table_with_strings_t = Table<"test_strings", Object4,
    Column<"id", &Object4::id, PrimaryKey<conflict_t::abort>>,
    Column<"opt_text", &Object4::opt_text>,
    Column<"text", &Object4::text>,
    Column<"more_text", &Object4::more_text, Default<"default">>,
    Column<"not_null_text", &Object4::not_null_text, NotNull<>>
      >;

using connection_t = Connection<table_t, tablepriv_t, table_with_column_constraints_t>;

class TableTest : public ::testing::Test {
    protected:
    void SetUp() override {
        auto created_conn = connection_t::create("test.db", 0, 0, &logger);
        my_conn = std::make_shared<connection_t>(std::move(created_conn));
    }

    std::shared_ptr<connection_t> my_conn;

    void TearDown() override {
        my_conn = nullptr;
        std::filesystem::remove("test.db");
    }
};


TEST_F(TableTest, n_columns) {
    ASSERT_EQ(table_t::n_columns, 2);
}

TEST_F(TableTest, create_table_query) {
    std::string query = table_t::create_table_query(false);
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    ASSERT_EQ(trimmed, "CREATE TABLE test ( `id` INTEGER NOT NULL ON CONFLICT ABORT, `name` TEXT DEFAULT '' ); ");
    std::string same = tablepriv_t::create_table_query(false);
    std::regex reg2 ("_private");
    same = std::regex_replace (same, reg2, "");
    ASSERT_EQ(same, query);
}

TEST_F(TableTest, CreateWithConstraintsTableQuery) {
    std::string query = table_with_column_constraints_t::create_table_query(false);
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    std::string expected = "CREATE TABLE test_constraints ( "
        "`id` INTEGER NOT NULL ON CONFLICT ABORT PRIMARY KEY ON CONFLICT ABORT, "
        "`name` TEXT NOT NULL ON CONFLICT ABORT UNIQUE ON CONFLICT ABORT, "
        "`text` TEXT DEFAULT '' UNIQUE ON CONFLICT REPLACE, "
        "`float` REAL NOT NULL ON CONFLICT ABORT, "
        "`someId` INTEGER NOT NULL ON CONFLICT ABORT REFERENCES `test` (`id`) ON UPDATE CASCADE ON DELETE RESTRICT "
        "); ";
    ASSERT_EQ(trimmed, expected);
}

TEST_F(TableTest, CreateWithStrings) {
    std::string query = table_with_strings_t::create_table_query(false);
    std::regex reg ("\\s+");
    auto trimmed = std::regex_replace (query, reg, " ");
    std::string expected = "CREATE TABLE test_strings ( "
        "`id` INTEGER NOT NULL ON CONFLICT ABORT PRIMARY KEY ON CONFLICT ABORT, "
        "`opt_text` TEXT, "
        "`text` TEXT DEFAULT '', "
        "`more_text` TEXT DEFAULT 'default', "
        "`not_null_text` TEXT NOT NULL ON CONFLICT ABORT "
        "); ";
    ASSERT_EQ(trimmed, expected);
}

