#!/usr/bin/env python

import time
from testgres import get_new_node

dictionary = (
    "ticker",
    "base",
    "target",
    "price",
    "volume",
    "change",
    "market",
    "timestamp",
    "success",
    "error",
)

create0 = """
create table t0(a json);
alter table t0 alter column a set storage external;
"""

create1 = """
create table t1(a json compression pglz);
"""

dict_str = ",".join(dictionary)
create2 = """
create extension pg_dict_compression;
create table t2(a json compression pg_dict_compression with (dict '%s'));
""" % dict_str

create3 = """
create table t3(a json compression zlib with (level 'best_compression'));
"""

insert_sql = """
insert into {0} values ('%s')
"""

class benchmark(object):
    def __init__(self, name, suit=None, suit_key=None):
        self.suit = suit
        self.suit_key = suit_key
        self.name = name

    def __enter__(self):
        self.start = time.time()

    def __exit__(self, ty, val, tb):
        self.end = time.time()
        self.diff = self.end - self.start

        print("%s : %0.3f seconds" % (self.name, self.diff))


with get_new_node("test1") as node:
    node.init().start()

    with open('trades.json', 'r') as f:
        doc = f.read()

    node.safe_psql(create0)
    node.safe_psql(create1)
    node.safe_psql(create2)
    node.safe_psql(create3)

    with node.connect() as con:
        with benchmark("insert uncompressed"):
            for i in range(10000):
                con.execute(insert_sql.format("t0") % doc)

        print("t0 size")
        print(node.safe_psql("select pg_size_pretty(pg_total_relation_size('t0'))"))

        with benchmark("insert pglz compressed"):
            for i in range(10000):
                con.execute(insert_sql.format("t1") % doc)

        print("t1 size")
        print(node.safe_psql("select pg_size_pretty(pg_total_relation_size('t1'))"))

        with benchmark("insert dict compressed"):
            for i in range(10000):
                con.execute(insert_sql.format("t2") % doc)

        print("t2 size")
        print(node.safe_psql("select pg_size_pretty(pg_total_relation_size('t2'))"))

        with benchmark("insert zlib compressed"):
            for i in range(10000):
                con.execute(insert_sql.format("t3") % doc)

        print("t3 size")
        print(node.safe_psql("select pg_size_pretty(pg_total_relation_size('t3'))"))
