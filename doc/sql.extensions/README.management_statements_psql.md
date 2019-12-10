# Management Statements in PSQL

Before Firebird 4, management statements were not allowed inside PSQL blocks. They were allowed only as top-level SQL statements, or as top-level statement of an `EXECUTE STATEMENT` embedded in a PSQL block.

Now they are allowed inside PSQL blocks (triggers, procedures, execute block) directly.

As many applications depends that some management statements be issued on the connection start, `ON CONNECT` triggers is a good place to put them in this situation.

The management statements part of this improvement are:
- `ALTER SESSION RESET`
- `SET BIND OF ... TO`
- `SET DECFLOAT ROUND`
- `SET DECFLOAT TRAPS TO`
- `SET ROLE`
- `SET SESSION IDLE TIMEOUT`
- `SET STATEMENT TIMEOUT`
- `SET TIME ZONE`
- `SET TRUSTED ROLE`

## Examples

```sql
create or alter trigger on_connect on connect
as
begin
    set bind of decfloat to double precision;
    set time zone 'America/Sao_Paulo';
end
```
