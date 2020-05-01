pg_subtrans_infos
===================

Features
--------

Allow to get subtransaction information thanks to a *pg_subtrans_infos* function.

Installation
============

Compiling
---------

The extension can be built using the standard PGXS infrastructure. For this to
work, the ``pg_config`` program must be available in your $PATH. Instruction to
install follows:

    $ git clone
    $ cd pg_subtrans_infos
    $ make
    $ make install
    $ psql DB -c "CREATE EXTENSION pg_subtrans_infos;"

Examples
=======

Example 1:
----------

```
postgres=# select txid_current_snapshot();
 txid_current_snapshot
-----------------------
 704:704:
(1 row)

postgres=# begin;
BEGIN
postgres=# insert into bdt values(1);
INSERT 0 1
postgres=# savepoint a;
SAVEPOINT
postgres=# insert into bdt values(1);
INSERT 0 1
postgres=# savepoint b;
SAVEPOINT
postgres=# insert into bdt values(1);
INSERT 0 1
postgres=# savepoint c;
SAVEPOINT
postgres=# insert into bdt values(1);
INSERT 0 1
postgres=# rollback to savepoint b;
ROLLBACK
postgres=# select * from pg_subtrans_infos(704);
 xid |   status    | parent_xid | top_parent_xid | sub_level | commit_timestamp
-----+-------------+------------+----------------+-----------+------------------
 704 | in progress |            |                |           |
(1 row)

postgres=# select * from pg_subtrans_infos(705);
 xid |   status    | parent_xid | top_parent_xid | sub_level | commit_timestamp
-----+-------------+------------+----------------+-----------+------------------
 705 | in progress |        704 |            704 |         1 |
(1 row)

postgres=# select * from pg_subtrans_infos(706);
 xid | status  | parent_xid | top_parent_xid | sub_level | commit_timestamp
-----+---------+------------+----------------+-----------+------------------
 706 | aborted |        705 |            704 |         2 |
(1 row)

postgres=# select * from pg_subtrans_infos(707);
 xid | status  | parent_xid | top_parent_xid | sub_level | commit_timestamp
-----+---------+------------+----------------+-----------+------------------
 707 | aborted |        706 |            704 |         3 |
(1 row)

postgres=# commit;
COMMIT
postgres=# select * from pg_subtrans_infos(704);
 xid |  status   | parent_xid | top_parent_xid | sub_level |       commit_timestamp
-----+-----------+------------+----------------+-----------+-------------------------------
 704 | committed |            |                |           | 2020-05-01 14:41:29.652714+00
(1 row)

postgres=# select * from pg_subtrans_infos(705);
 xid |  status   | parent_xid | top_parent_xid | sub_level |       commit_timestamp
-----+-----------+------------+----------------+-----------+-------------------------------
 705 | committed |        704 |                |           | 2020-05-01 14:41:29.652714+00
(1 row)
```

Example 2:
----------
```
postgres=# \! cat lock_and_subtrans_infos.sql
select
        pid,
        locktype,
        mode,
        (select xid from pg_subtrans_infos(pgl.transactionid::text::bigint)),
        (select status from pg_subtrans_infos(pgl.transactionid::text::bigint)) as "xid status",
        (select parent_xid from pg_subtrans_infos(pgl.transactionid::text::bigint)),
        (select top_parent_xid from pg_subtrans_infos(pgl.transactionid::text::bigint)),
        (select sub_level from pg_subtrans_infos(pgl.transactionid::text::bigint)) sub_level,
        (select commit_timestamp from pg_subtrans_infos(pgl.transactionid::text::bigint))
from
(select * from pg_locks where transactionid is not null) pgl
order by 4;

postgres=# \i lock_and_subtrans_infos.sql
  pid  |   locktype    |     mode      | xid | xid status  | parent_xid | top_parent_xid | sub_level | commit_timestamp
-------+---------------+---------------+-----+-------------+------------+----------------+-----------+------------------
 13735 | transactionid | ExclusiveLock | 712 | in progress |            |                |           |
 13735 | transactionid | ExclusiveLock | 713 | in progress |        712 |            712 |         1 |
 13735 | transactionid | ExclusiveLock | 714 | in progress |        713 |            712 |         2 |
 13735 | transactionid | ExclusiveLock | 715 | in progress |        714 |            712 |         3 |
(4 rows)
```

Remarks
=======
* top_parent_xid could be empty when status is not "in progress"
* has been tested from version 10 to 12.2

License
=======

pg_subtrans_infos is free software distributed under the PostgreSQL license.

Copyright (c) 2020, Bertrand Drouvot.
