Note to self: start from top

* Join
    Here are some notes you wrote to yourself before going to England for a week:

    Need to test using a `where` that targets the join table
    e.g.
    ```cpp
    connection.where<Table>(JoinTable::field<"something">)
        .join<JoinTable::field<"t_id">, Table::field<"id">>().one();
    ```

    The join fields should also be derivable if we know the table and there
    is a foreign key.
    e.g.
    ```cpp
    connection.where<Table>(JoinTable::field<"something">).join<JoinTable>().one();
    ```

    OUTER JOIN is currently useless, since the selected columns are not dynamic

    Should get rid of `all` and replace it with a general `select` method that
    allows specifying more than table to be returned

    The new `select` method should automatically generate the `join` clause if more
    than one table is selected, and it is possible to determine the join based on
    the foreign key
    e.g
    ```cpp
    connection.select<FromTable, JoiningTable>().many();
    ```

    Also, the above should work for more than one join too (joining tables is variadic)
    This means making the `join` clause on the `Query` a vector of clauses

* Syntax errors, more detail than "Logic error"

* Statement caching

* REGEXP

* Docs

* Bulk update

* Relationships

* Cleanup template errors

* Deferrable Foreign keys

* Constraints:
    * Check
    * Expression as default

* Table constraints

* Tests:
    * Constraints
    * Transactions

FIX:
    duplicate column class
