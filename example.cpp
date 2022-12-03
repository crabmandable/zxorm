/*
 * To compile this example on linux:
 *    g++ example.cpp -Iincludes -o example.bin `pkg-config --libs sqlite3` -std=c++20
 */
#include "zxorm/zxorm.hpp"

using namespace zxorm;

struct Student {
    enum Year {
        Freshman = 1,
        Sophmore = 2,
        Senior = 3,
    };

    // row id (primary key) field
    int id = 0;

    Year year = Year::Freshman;

    std::string name;

    // newly enrolled students don't have a GPA yet,
    // hence it is optional
    std::optional<float> gpa;
};

using StudentTable = Table<"students", Student,
    Column<"id", &Student::id, PrimaryKey<>>,
    Column<"name", &Student::name>,
    Column<"year", &Student::year>,
    Column<"gpa", &Student::gpa>
        >;

int main (void) {
    auto maybe_connection = Connection<StudentTable>::create("school.db");

    // most zxorm functions return a `zxorm::Result` template, that must be unwrapped
    if (maybe_connection.is_error()) {
        throw std::runtime_error("Unable to open school database");
    }

    auto connection = std::move(maybe_connection.value());

    // now we have a connection, we can create the tables if they don't exist
    connection.create_tables();

    // this will run the following query:
    /*
     * CREATE TABLE IF NOT EXIST students (
     *     `id` INTEGER NOT NULL ON CONFLICT ABORT PRIMARY KEY ON CONFLICT ABORT,
     *     `name` TEXT NOT NULL ON CONFLICT ABORT,
     *     `year` INTEGER NOT NULL ON CONFLICT ABORT,
     *     `gpa` REAL
     * );
     */

    // and now we can start saving some students
    auto newStudent = Student();
    newStudent.name = "zach";
    connection.insert_record(newStudent);

    // find a record by its primary key
    auto zach = connection.find_record<Student>(newStudent.id);
    if (zach.is_error() || !zach.has_value()) {
        throw std::runtime_error("Couldn't find zach");
    }

    // update a record
    zach.value().gpa = 3.14;
    connection.update_record(zach.value());
}
