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
 661:661:
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

postgres=# select * from pg_subtrans_infos(661);                                                                         [30/1802]
 xid |   status    | parent_xid | top_parent_xid | commit_timestamp
-----+-------------+------------+----------------+------------------
 661 | in progress |            |                |
(1 row)

postgres=# select * from pg_subtrans_infos(662);
 xid |   status    | parent_xid | top_parent_xid | commit_timestamp
-----+-------------+------------+----------------+------------------
 662 | in progress |        661 |            661 |
(1 row)

postgres=# select * from pg_subtrans_infos(663);
 xid | status  | parent_xid | top_parent_xid | commit_timestamp
-----+---------+------------+----------------+------------------
 663 | aborted |        662 |            661 |
(1 row)

postgres=# select * from pg_subtrans_infos(664);
 xid | status  | parent_xid | top_parent_xid | commit_timestamp
-----+---------+------------+----------------+------------------
 664 | aborted |        663 |            661 |

postgres=# commit;
COMMIT

postgres=# select * from pg_subtrans_infos(661);
 xid |  status   | parent_xid | top_parent_xid |       commit_timestamp
-----+-----------+------------+----------------+------------------------------
 661 | committed |            |                | 2020-05-01 10:04:06.62758+00
(1 row)

postgres=# select * from pg_subtrans_infos(662);
 xid |  status   | parent_xid | top_parent_xid |       commit_timestamp
-----+-----------+------------+----------------+------------------------------
 662 | committed |        661 |                | 2020-05-01 10:04:06.62758+00
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
        (select xid from pg_subtrans_infos(pgl.transactionid::text::bigint)) - (select top_parent_xid from pg_subtrans_infos(pgl.transactionid::text::bigint)) sublevel,
        (select commit_timestamp from pg_subtrans_infos(pgl.transactionid::text::bigint))
from
(select * from pg_locks where transactionid is not null) pgl
order by 4;

postgres=# \i lock_and_subtrans_infos.sql
  pid  |   locktype    |     mode      | xid | xid status  | parent_xid | top_parent_xid | sublevel | commit_timestamp
-------+---------------+---------------+-----+-------------+------------+----------------+----------+------------------
 11145 | transactionid | ExclusiveLock | 673 | in progress |            |                |          |
 11145 | transactionid | ExclusiveLock | 674 | in progress |        673 |            673 |        1 |
 11145 | transactionid | ExclusiveLock | 675 | in progress |        674 |            673 |        2 |
 11145 | transactionid | ExclusiveLock | 676 | in progress |        675 |            673 |        3 |
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
