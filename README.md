# ZxORM
**Z**ach's **O**bject **R**elational **M**apping library - A C++20 ORM for SQLite
___

`zxorm` is an attempt to create an ORM for C++ that achieves the following:
* Compile time type safety
* No code generation
* Low amount of boilerplate code
* No requirement to inherit from anything or conform to a prescribed interface
* SQL like syntax for writing more complicated queries without writing raw SQL


### Getting started
1. Write your objects

```cpp
struct Object {
    int id = 0;
    std::string some_text;
};

```
`zxorm` doesn't provide a base class or anything like that. The types for the
SQL table are instead inferred from types used in the C++ code.

2. Create a `Table` for your object
```cpp
using ObjectTable = zxorm::Table<"objects", Object,
    zxorm::Column<"id", &Object::id, zxorm::PrimaryKey<>>,
    zxorm::Column<"some_text", &Object::some_text>
>;
```
This is the mapping that will tell `zxorm` how to deal with `Object` and what table
in the database it is referring to.

3. Create a connection
```cpp
auto result = zxorm::Connection<ObjectTable>::create("data.db");
assert(!result.is_error());
auto connection = std::move(result.value());
```

4. Start running queries
```cpp
connection.create_tables();
connection.insert_many_records(std::vector<Object> {
    { .some_text = "This is my data" },
    { .some_text = "Wow it is so important" },
    { .some_text = "Better make sure it is saved" },
    { .some_text = "!" },
});
auto result = connection.find_record<Object>(4);
assert(!result.is_error() && result.has_value());
assert(result.value().some_text == "!");
```

Check out the [example](./example.cpp) for more
___

### Why did I write this?

I created this library after looking at what was available in C++ and seeing that
the options for a true ORM library are quite limited.

Many C++ programmers will tell you that an ORM is simply bloat that will slow
your performance, and they're not entirely wrong. However, there is a reason
why they proliferate in higher level languages: they are incredibly valuable
for the maintainability of large projects. `zxorm` is an attempt to have our cake
and eat it.

Almost all C++ ORMs are built using traditional object oriented paradigms, without
making much use of modern C++ metaprogramming techniques. C++20 introduced many
useful metaprogramming features into the language, which `zxorm` utilizes to
generate queries with compile-time type checking.

Much influence was taken from the excellent [`sqlpp11`](https://github.com/rbock/sqlpp11)
library, however it is not a true ORM, and requires code generation, or manually
writing a lot of boilerplate.

I wanted to write something that is simple to integrate, easy to start using, and
totally agnostic to how the `Object` in the `ORM` is written.

___
### Caching queries

The basic queries such as `find_record`, `insert_record` and `delete_record` will
use statement caching, meaning that the query string does not need to be regenerated,
and the statement doesn't need to be recompiled by the underlying SQL engine.

This is possible since the shape of these queries, and the number of binds
never changes. For more open ended queries, caching and reuse is possible,
but it is up to **you** to implement.

It is very important to note that when reusing queries, it is not possible to
change the text of a query that was already executed, it can only be bound with
different parameters. If a query is reused with a different where clause,
then undefined behaviour may occur.
___

### Multithreading

This is currently not well tested but in theory it should work fine as long as
you follow the golden rule:

**:warning: 1 connection per thread**
