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
    auto found = connection.find_record<Student>(newStudent.id);
    if (found.is_error() || !found.has_value()) {
        throw std::runtime_error("Couldn't find new student");
    }

    // update a record
    found.value().gpa = 3.14;
    connection.update_record(found.value());

    // find a record by some other column
    auto zach = connection.where<Student>(StudentTable::field<"name">.like("zach")).one();
    if (zach.is_error() || !zach.has_value()) {
        throw std::runtime_error("Couldn't find zach");
    }

    connection.insert_many_records(std::vector<Student> {
        { .year = Student::Year::Freshman, .name = "jojo", .gpa = 3.44 },
        { .year = Student::Year::Sophmore, .name = "janet", .gpa = 2.4 },
        { .year = Student::Year::Sophmore, .name = "bob", .gpa = 3.9 },
        { .year = Student::Year::Senior, .name = "billie", .gpa = 3.95 },
        { .year = Student::Year::Senior, .name = "wayne", .gpa = 2.98 },
    });

    // find many records with a more complicated WHERE clause
    auto students = connection.where<Student>(
        StudentTable::field<"gpa"> >= 3.0 &&
        StudentTable::field<"year"> > Student::Year::Freshman
    ).many();

    std::cout << "passing students:\n";
    for (const Result<Student>& student: students.value()) {
        std::cout << "\t" << student.value().name << "\n";
    }
}
