---------------
Partial indices
---------------

  Function:
    Allow to index only a subset of table rows defined by the search condition specified during index creation.

  Author:
    Dmitry Yemanov <dimitr@firebirdsql.org>

  Syntax rules:
    CREATE [UNIQUE] [{ASC[ENDING] | DESC[ENDING]}] INDEX <index_name> ON <table_name>
      { (<column_list>) | COMPUTED [BY] ( <value_expression> ) }
      WHERE <search_condition>

  Scope:
    DSQL (DDL)

  Example(s):
    1. CREATE INDEX IT1_COL ON T1 (COL) WHERE COL < 100;
       SELECT * FROM T1 WHERE COL < 100;
       -- PLAN (T1 INDEX (IT1_COL))
    2. CREATE INDEX IT1_COL2 ON T1 (COL) WHERE COL IS NOT NULL;
       SELECT * FROM T1 WHERE COL > 100;
       -- PLAN (T1 INDEX IT1_COL2)
    3. CREATE INDEX IT1_COL3 ON T1 (COL) WHERE COL = 1 OR COL = 2;
       SELECT * FROM T1 WHERE COL = 2;
       -- PLAN (T1 INDEX IT1_COL3)

  Note(s):
    1. A partial index definition may include the UNIQUE specification. In this case,
       every key in the index is required to be unique. This allows to enforce uniqueness
       across some subset of table rows.
    2. Partial index is usable only in the following cases:
       a) WHERE condition includes exactly the same boolean expression as the one
          defined for the index;
       b) Search condition defined for the index contains ORed boolean expressions
          and one of them is explicitly included in the WHERE condition;
       c) Search condition defined for the index specifies IS NOT NULL and the
          WHERE condition includes an expression on the same field that is known to
          ignore NULLs.

