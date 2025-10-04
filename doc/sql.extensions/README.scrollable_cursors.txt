------------------
Scrollable cursors
------------------

  Function:
    Allow to navigate through the open cursor in any direction rather than sequentially fetching records forward.

  Author:
    Dmitry Yemanov <dimitr@firebirdsql.org>

  Scope:
    PSQL, DSQL with additional API support

  Syntax rules (PSQL):
    // To declare a cursor:
    DECLARE <name> SCROLL CURSOR FOR ( <select expression> )

    // To fetch forward:
    FETCH <cursor name> [INTO <var name> [, <var name> ...]];

    // To fetch in any direction:
    FETCH {NEXT | PRIOR | FIRST | LAST | ABSOLUTE <n> | RELATIVE <n>} FROM <cursor name> [INTO <var name> [, <var name> ...]];

    See also /doc/sql.extensions/README.cursors

  Usage with API:
    Result set must be opened with the flag IStatement::CURSOR_TYPE_SCROLLABLE explicitly specified.
    The following fetch methods of the IResultSet interface are available:

      int fetchNext(IStatus* status, void* message);
      // equal to FETCH NEXT FROM <cursor name>

        Moves the cursor's current position to the next row and return it.
        If cursor is empty or already positioned at the last row, NO_DATA condition is returned.

      int fetchPrior(IStatus* status, void* message);
      // equal to FETCH PRIOR FROM <cursor name>

        Moves the cursor's current position to the prior row and return it.
        If cursor is empty or already positioned at the first row, NO_DATA condition is returned.

      int fetchFirst(IStatus* status, void* message);
      // equal to FETCH FIRST FROM <cursor name>

        Moves the cursor's current position to the first row and return it.
        If cursor is empty, NO_DATA condition is returned.

      int fetchLast(IStatus* status, void* message);
      // equal to FETCH LAST FROM <cursor name>

        Moves the cursor's current position to the last row and return it.
        If cursor is empty, NO_DATA condition is returned.

      int fetchAbsolute(IStatus* status, int position, void* message);
      // equal to FETCH ABSOLUTE <position> FROM <cursor name>

        Moves the cursor's current position to the specified one and return the located row.
        If specified position is beyond the cursor's boundaries, NO_DATA condition is returned.

      int fetchRelative(IStatus* status, int offset, void* message);
      // equal to FETCH RELATIVE <offset> FROM <cursor name>

        Moves the cursor's current position backward or forward by the specified offset and return the located row.
        If calculated position is beyond the cursor's boundaries, NO_DATA condition is returned.

  Note(s):
    1. When a scrolling option is omitted, NO SCROLL is implied (ie. cursor is opened as forward-only).
       In PSQL, this means that only FETCH [NEXT FROM] commands can be used, other commands will return an error.
       For API users, this means that only fetchNext() API call can be used, other fetch methods will return an error.
    2. Scrollable cursors are internally materialized as a temporary record set,
       thus consuming memory/disk resources, so this feature should be used only when really necessary.
