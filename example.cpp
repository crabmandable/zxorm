/*
 * To compile this example on linux:
 *    g++ example.cpp -Iincludes -o example.bin `pkg-config --libs sqlite3` -std=c++20
 */
#include "zxorm/zxorm.hpp"
#include <iomanip>

using namespace zxorm;

/**
 * Student - This is the "Object" in ORM for this example
 *
 *           Nothing special goin on here, just a class/struct
 *           that represents a row in our db table
 */
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

    // newly enrolled students don't have a GPA yet, hence it is optional
    // optional will mean that the inferred SQL type is allowed to be NULL
    std::optional<float> gpa;
};

/**
 * StudentTable - This typedef is the "schema" for the table 'students'
 *
 *                Using `zxorm::Table`, it tells zxorm how to map each column
 *                onto the struct `Student`.
 *
 *                The type of each column is inferred by the type of the member.
 *
 *                Column constraints are also added here
 */
using StudentTable = Table<"students", Student,
    Column<"id", &Student::id, PrimaryKey<>>,
    Column<"name", &Student::name>,
    Column<"year", &Student::year>,
    Column<"gpa", &Student::gpa>
        >;


int main (void) {
    // Pass all the tables that are part of our schema to the connection object
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

    // Clear out any old data
    connection.truncate<Student>();

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
    auto zach = connection.select_query<StudentTable>()
        .where(StudentTable::field_t<"name">().like("zach"))
        .one();

    if (zach.is_error() || !zach.has_value()) {
        throw std::runtime_error("Couldn't find zach");
    }

    using Year = Student::Year;

    connection.insert_many_records(std::vector<Student> {
        { .year = Year::Freshman, .name = "jojo",    .gpa = 3.44 },
        { .year = Year::Sophmore, .name = "janet",   .gpa = 2.4  },
        { .year = Year::Sophmore, .name = "bob",     .gpa = 3.9  },
        { .year = Year::Senior,   .name = "billie",  .gpa = 3.95 },
        { .year = Year::Senior,   .name = "wayne",   .gpa = 2.98 },
        { .year = Year::Freshman, .name = "charlie", .gpa = 1.3 },
        { .year = Year::Senior,   .name = "mac",     .gpa = 1.0 },
        { .year = Year::Senior,   .name = "dee",     .gpa = 2.99 },
        { .year = Year::Senior,   .name = "dennis",  .gpa = 3.1 },
    });

    constexpr auto PASS_GPA = 3.0f;

    // find many records with a more complicated WHERE clause
    auto students = connection.select_query<StudentTable>()
        .where(StudentTable::field_t<"gpa">() >= PASS_GPA &&
               StudentTable::field_t<"year">() >= Year::Sophmore)
        .many();

    std::cout << "Students who have a passing GPA >= "
        << std::fixed << std::setprecision(1) << PASS_GPA
        << ":\n";

    for (const Result<Student>& student: students.value()) {
        std::cout << "\t" << student.value().name << "\n";
    }

    // simple count query
    auto n_failing = connection.select_query<CountAll, From<StudentTable>>()
        .where(StudentTable::field_t<"gpa">() < PASS_GPA)
        .one();

    std::cout << "There are " << n_failing.value() << " failing students in all years\n";


    // more complicated counting using a GroupBy clause
    auto count_result = connection.select_query<
        Select<Count<Student>, StudentTable::field_t<"year">>
    >().group_by<StudentTable::field_t<"year">>().many();

    for (const auto& row : count_result.value()) {
        // each row will be represented by a tuple, according to the `Select` clause
        // here, the first element will be the count column,
        // and the second element will be the year
        std::cout << "There are " << std::get<0>(row.value()) << " students "
            "in year " << std::get<1>(row.value()) << "\n";
    }
}
