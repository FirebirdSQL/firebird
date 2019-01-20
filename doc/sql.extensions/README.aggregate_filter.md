# FILTER clause for aggregate functions

The `FILTER` clause for aggregate function is a shortcut for when an aggregate function is used with some condition (decode, case, iif) to disconsider some values in the aggregation.

The clause could be used with all aggregate functions in aggregate or windowed (`OVER`) calls, but cannot be used with window-only functions like `DENSE_RANK`.

For example, if in the same query there is a need to count the number of status = 'A' and the number of status = 'E' as different columns, the old way to do it is:

```sql
select count(decode(status, 'A', 1)) status_a,
       count(decode(status, 'E', 1)) status_e
  from data;
```

With `FILTER` that conditions could be more explicit:

```sql
select count(*) filter (where status = 'A') status_a,
       count(*) filter (where status = 'E') status_e
  from data;
```

## Syntax

```
aggregate_function [FILTER (WHERE <condition>)] [OVER (<window>)]
```
