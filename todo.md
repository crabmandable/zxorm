Note to self: start from top

* Join
    Here are some notes you wrote to yourself before going to England for a week:

    Test: delete with join

    Test joining many tables

    - Make the record iterator handle tuples (many)

    - Make outer/cross join work (need to make returned objects optional)

    - Prevent bad queries that result from selecting tables that aren't in the query

    - `select` method should automatically generate the `join` clause if more
      than one table is selected, and it is possible to determine the join based on
      the foreign key
        e.g
        ```cpp
        connection.select<FromTable, JoiningTable>().many();
        ```

    - Nested joins!

* Allow using string names, or table classes instead of only objects for query methods

* Count with query

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
