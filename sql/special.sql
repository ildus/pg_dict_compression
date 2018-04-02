CREATE SCHEMA test;
CREATE EXTENSION pg_dict_compression SCHEMA test;
CREATE TABLE test.c1(
	a BYTEA COMPRESSION pg_dict_compression WITH (dict 'german he she her budget long')
);

CREATE OR REPLACE FUNCTION test.dict_compression_test(OID, BYTEA)
RETURNS BYTEA
AS 'pg_dict_compression', 'dict_compression_test'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION test.dict_compression_test_wrapper(input BYTEA)
RETURNS BYTEA AS $$
DECLARE
	cmid OID;
	res  BYTEA;
BEGIN
	SELECT acoid FROM pg_attr_compression INTO cmid WHERE acrelid = 'test.c1'::regclass;
	res := test.dict_compression_test(cmid, input);
	RETURN res;
END
$$ LANGUAGE plpgsql;

SELECT test.dict_compression_test_wrapper(E'\\x6865FF736865'); /* 6 */
SELECT test.dict_compression_test_wrapper(E'\\x627564676572FF6d616e');	/* null */
SELECT test.dict_compression_test_wrapper(E'\\x627564676572FF6765726d616e');	/* 10 */

DROP SCHEMA test CASCADE;
