Note to self: start from top

* Join
    Here are some notes you wrote to yourself before going to England for a week:

    Test: delete with join

    OUTER JOIN is currently useless, since the selected columns are not dynamic

    Should get rid of `all` and replace it with a general `select` method that
    allows specifying more than one table to be returned

    The new `select` method should automatically generate the `join` clause if more
    than one table is selected, and it is possible to determine the join based on
    the foreign key
    e.g
    ```cpp
    connection.select<FromTable, JoiningTable>().many();
    ```

    Also, the above should work for more than one join too (joining tables is variadic)
    This means making the `join` clause on the `Query` a vector of clauses

* Allow using string names, or table classes instead of only objects for query methods

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
