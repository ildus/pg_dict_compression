CREATE EXTENSION pg_dict_compression;
CREATE TABLE c1(
	a TEXT COMPRESSION pg_dict_compression WITH (dict 'german he she her budget long')
);
SELECT length(repeat('0\0xFFhe00sher00lonshe00lohe00herhe00budgerman00\0xFF', 100)) as len;
INSERT INTO c1 VALUES (repeat('0\0xFFhe00sher00lonshe00lohe00herhe00budgerman00\0xFF', 100));
SELECT length(a) as len, a = repeat('0\0xFFhe00sher00lonshe00lohe00herhe00budgerman00\0xFF', 100) AS is_equal FROM c1;

CREATE OR REPLACE FUNCTION dict_compression_test(OID, TEXT)
RETURNS TEXT
AS 'pg_dict_compression', 'dict_compression_test'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION dict_compression_test_wrapper(input TEXT)
RETURNS TEXT AS $$
DECLARE
	cmid OID;
	res  TEXT;
BEGIN
	SELECT acoid FROM pg_attr_compression INTO cmid WHERE acrelid = 'c1'::regclass;
	res := dict_compression_test(cmid, input);
	RETURN res;
END
$$ LANGUAGE plpgsql;

SELECT dict_compression_test_wrapper('shehe'); /* 4 */
SELECT dict_compression_test_wrapper('budgerman');	/* 5 */
SELECT dict_compression_test_wrapper('budgerman00she');	/* 5 + 2 + 2 */
SELECT dict_compression_test_wrapper('sh');	/* 2 */
SELECT dict_compression_test_wrapper('lon00');	/* 5 */
SELECT dict_compression_test_wrapper('long00');	/* 4 */
SELECT dict_compression_test_wrapper(
	repeat('0\0xFFhe00sher00lonshe00lohe00herhe00budgerman00\0xFF', 100)) =
	repeat('0\0xFFhe00sher00lonshe00lohe00herhe00budgerman00\0xFF', 100);

SELECT dict_compression_test_wrapper('{"budget": 1, "german": 2}'::json::text)::json;

DROP TABLE c1;

-- params check
CREATE TABLE c1(a text);
ALTER TABLE c1 ALTER COLUMN a SET COMPRESSION pg_dict_compression;
ALTER TABLE c1 ALTER COLUMN a SET COMPRESSION pg_dict_compression WITH (alt 'one');
ALTER TABLE c1 ALTER COLUMN a SET COMPRESSION pg_dict_compression WITH (dict 'o');
ALTER TABLE c1 ALTER COLUMN a SET COMPRESSION pg_dict_compression WITH (dict '');

DROP EXTENSION pg_dict_compression;
