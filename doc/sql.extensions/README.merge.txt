-----------------
MERGE statement
-----------------

  Function:
    Read data from the source and INSERT, UPDATE or DELETE in the target table depending on a
    condition.

  Author:
    Adriano dos Santos Fernandes <adrianosf@gmail.com>

  Format:
	<merge statement> ::=
		MERGE
			INTO <table or view> [ [AS] <correlation name> ]
			USING <table or view or derived table> [ [AS] <correlation name> ]
			ON <condition>
			<merge when>...
			[<plan clause>]
			[<order by clause>]
			[<returning clause>]

	<merge when> ::=
		<merge when matched> |
		<merge when not matched by target> |
		<merge when not matched by source>

	<merge when matched> ::=
		WHEN MATCHED [ AND <condition> ] THEN
			{ UPDATE SET <assignment list> | DELETE }

	<merge when not matched by target> ::=
		WHEN NOT MATCHED [ BY TARGET ] [ AND <condition> ] THEN
			INSERT [ <left paren> <column list> <right paren> ]
				VALUES <left paren> <value list> <right paren>

	<merge when not matched by source> ::=
		WHEN NOT MATCHED BY SOURCE [ AND <condition> ] THEN
			{ UPDATE SET <assignment list> | DELETE }

  Syntax rules:
	1. At least one <merge when> clause should be specified.

  Scope:
    DSQL, PSQL

  Examples:
	1.
		MERGE
			INTO customers c
			USING (SELECT * FROM customers_delta WHERE id > 10) cd
			ON (c.id = cd.id)
			WHEN MATCHED THEN
				UPDATE SET
					name = cd.name
			WHEN NOT MATCHED THEN
				INSERT (id, name)
					VALUES (cd.id, cd.name)

	2.
		MERGE
			INTO customers c
			USING new_customers nc
			ON (c.id = nc.id)
			WHEN MATCHED THEN
				UPDATE SET
					name = cd.name
			WHEN NOT MATCHED BY SOURCE THEN
				DELETE

  Notes:
	A join is made between USING and INTO tables.

	The join type depends on the presence of
	<merge when not matched by source> and <merge when not matched by target>:
	- <merge when not matched by target> + <merge when not matched by source>: FULL JOIN
	- <merge when not matched by source>: RIGHT JOIN
	- <merge when not matched by target>: LEFT JOIN
	- only <merge when matched>: INNER JOIN

	As soon it's decided if the source and target has a matching, the set of the
	corresponding (WHEN MATCHED / WHEN NOT MATCHED) statements is evaluated in the order specified,
	to check their optional conditions. The first statement which has its condition evaluated to true
	is the one which will be executed, and the subsequent ones will be ignored.

	If no record is returned in the join, no action will be called.

	<merge when matched> is called when a match between source and target exists.
	UPDATE or DELETE will change the target table.

	<merge when not matched by target> is called when a source record matches no record in target.
	INSERT will change the target table.

	<merge when not matched by source> is called when a target record matches no record in source.
	UPDATE or DELETE will change the target table.
	That clause is allowed only since v5.
