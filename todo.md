Note to self: start from top

* More count tests:
    count with where

* BUG: Order doesn't make sense, needs to take in a field instead of a string name

* BUG: count query returns optional because `one` is always optional

* BUG: `Join` should allow automatically joining by foreign key if the foreign key
    is on the join table, instead of the `From` table

* Docs

* Enforce type safety for where expressions

* Statement caching

* REGEXP

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
