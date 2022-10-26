# SKIP LOCKED clause (FB 5.0)

`SKIP LOCKED` clause can be used with `SELECT ... WITH LOCK`, `UPDATE` and `DELETE` statements.

It makes engine skip records locked by others transactions instead of wait on them or raise conflict errors.

This is very useful to implement work queues where one or more processes post work to a table and issue
an event while workers listen for events and read/delete items from the table. Using `SKIP LOCKED` multiple
workers can get exclusive work items from the table without conflicts.

## Syntax

```
SELECT
  [FIRST ...]
  [SKIP ...]
  FROM <sometable>
  [WHERE ...]
  [PLAN ...]
  [ORDER BY ...]
  [{ ROWS ... } | {OFFSET ...} | {FETCH ...}]
  [FOR UPDATE [OF ...]]
  [WITH LOCK [SKIP LOCKED]]
```

```
UPDATE <sometable>
  SET ...
  [WHERE ...]
  [PLAN ...]
  [ORDER BY ...]
  [ROWS ...]
  [SKIP LOCKED]
  [RETURNING ...]
```

```
DELETE FROM <sometable>
  [WHERE ...]
  [PLAN ...]
  [ORDER BY ...]
  [ROWS ...]
  [SKIP LOCKED]
  [RETURNING ...]
```

## Notes

As it happens with subclauses `FIRST`/`SKIP`/`ROWS`/`OFFSET`/`FETCH` record lock
(and "skip locked" check) is done in between of skip (`SKIP`/`ROWS`/`OFFSET`/`FETCH`) and
limit (`FIRST`/`ROWS`/`OFFSET`/`FETCH`) checks.

## Examples

### Prepare metadata

```
create table emails_queue (
    subject varchar(60) not null,
    text blob sub_type text not null
);

set term !;

create trigger emails_queue_ins after insert on emails_queue
as
begin
    post_event('EMAILS_QUEUE');
end!

set term ;!
```

### Sender application or routine

```
insert into emails_queue (subject, text)
  values ('E-mail subject', 'E-mail text...');
commit;
```

### Client application

Client application can listen to event `EMAILS_QUEUE` to actually send e-mails using this query:

```
delete from emails_queue
  rows 10
  skip locked
  returning subject, text;
```

More than one instance of the application may be running, for example to load balance work.
