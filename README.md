# pg_dict_compression extension

Dictionary compression for PostgreSQL

## Requirements

This extension only works with developement version of PostgreSQL with custom compression methods patch applied:
https://www.postgresql.org/message-id/20180115024930.48583c69@hh

## Installation

To install `pg_dict_compression` extension run:

```
make install
```

or if your PostgreSQL bin directory isn't in PATH:

```
make install PG_CONFIG=<path_to>/pg_config
```

## Usage

First, create an extension:

```
CREATE EXTENSION pg_dict_compression;
```

Now you can specify the compression when defining new tables:

```
CREATE TABLE mytable (
	id SERIAL,
	data1 TEXT COMPRESSION pg_dict_compression WITH (dict 'one two three'),
	data2 TEXT
);
```

or enable compression for existing table columns:

```
ALTER TABLE mytable ALTER COLUMN data2 SET COMPRESSION pg_dict_compression WITH (dict 'four five six');
```
