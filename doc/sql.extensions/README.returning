----------------
RETURNING clause
----------------

  Function:
    Allow to return the column values actually stored in the table as a result of the "INSERT",
    "UPDATE OR INSERT", UPDATE, DELETE and MERGE statements.
    The most common usage is to retrieve the value of the primary key generated inside a BEFORE-trigger.

  Authors:
    Dmitry Yemanov <yemanov@yandex.ru>
    Adriano dos Santos Fernandes <adrianosf@uol.com.br>

  Syntax rules:
    INSERT INTO ... VALUES (...) [RETURNING <returning_list> [INTO <variable_list>]]
    INSERT INTO ... SELECT ... [RETURNING <returning_list> [INTO <variable_list>]]
    UPDATE OR INSERT INTO ... VALUES (...) ... [RETURNING <returning_list> [INTO <variable_list>]]
    UPDATE ... [RETURNING <returning_list> [INTO <variable_list>]]
    DELETE FROM ... [RETURNING <returning_list> [INTO <variable_list>]]
    MERGE INTO ... [RETURNING <returning_list> [INTO <variable_list>]]

    <returning_list> ::= <expr> [[AS] <alias>] {, <expr> [[AS] <alias>]}...

  Scope:
    DSQL, PSQL

  Example(s):
    1. INSERT INTO T1 (F1, F2)
       VALUES (:F1, :F2)
       RETURNING F1, F2 INTO :V1, :V2;
    2. INSERT INTO T2 (F1, F2)
       VALUES (1, 2)
       RETURNING ID INTO :PK;
    3. DELETE FROM T1 WHERE F1 = 1 RETURNING F2;
    4. UPDATE T1 SET F2 = F2 * 10 RETURNING OLD.F2, NEW.F2;
    5. UPDATE T1 SET F2 = F2 * 10 RETURNING OLD.F2 OLD_F2, NEW.F2 AS NEW_F2;
    6. UPDATE T1 SET F2 = F2 * 10 WHERE CURRENT OF C RETURNING NEW.F2;

  Note(s):
    1. The INTO part (i.e. the variable list) is allowed in PSQL only (to assign local variables)
       and rejected in DSQL.
    2. In DSQL, INSERT INTO ... VALUES are returned within the same protocol roundtrip as the
       INSERT is executed - this is not the case for INSERT INTO ... SELECT.
    3. If the RETURNING clause is present, then the statement is described as
       isc_info_sql_stmt_exec_procedure by the API (for INSERT INTO ... VALUES and statements
       with WHERE CURRENT OF) and isc_info_sql_stmt_select for the others statements, so the
       existing connectivity drivers should support this feature automagically.
    4. Any explicit record change (update or delete) performed by AFTER-triggers is ignored by
       the RETURNING clause.
    5. OLD and NEW could be used in RETURNING clause of UPDATE, INSERT OR UPDATE and MERGE statements.
    6. In UPDATE and INSERT OR UPDATE statements, unqualified or qualified by table name/alias
       fields are resolved to NEW.
    7. In MERGE WHEN MATCHED UPDATE and MERGE WHEN NOT MATCHED statements, unqualified or qualified
       by table name/alias fields are resolved to NEW. In MERGE WHEN MATCHED DELETE they are
       resolved to OLD.
    8. Since v4 it's possible to use * and alias.*, as well OLD.* and NEW.* where applicable.
    9. ORDER BY works with fields from the OLD context (the original record source before changes are applied).

  Limitations:
    1. The modify statement (INSERT, UPDATE, DELETE, UPDATE OR INSERT, MERGE) in PSQL should
       operate in no more than one record (i.e. should be singleton).

  Changes in v5:
    1. Ability to return multiple rows (or no row) in DSQL.
    2. Added PLAN and ORDER BY subclauses to MERGE.
	3. Added PLAN, ORDER BY and ROWS subclauses to UPDATE OR INSERT.
