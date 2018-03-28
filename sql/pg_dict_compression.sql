CREATE EXTENSION pg_dict_compression;
CREATE TABLE c1(
	a TEXT COMPRESSION pg_dict_compression WITH (dict 'german he she her long')
);
SELECT length(repeat('0\0xFFhe00sher00lonshe00lohe00herhe00longerman00\0xFF', 100)) as len;
INSERT INTO c1 VALUES (repeat('0\0xFFhe00sher00lonshe00lohe00herhe00longerman00\0xFF', 100));
SELECT length(a) as len, a = repeat('0\0xFFhe00sher00lonshe00lohe00herhe00longerman00\0xFF', 100) AS is_equal FROM c1;

DROP TABLE c1;
DROP EXTENSION pg_dict_compression;