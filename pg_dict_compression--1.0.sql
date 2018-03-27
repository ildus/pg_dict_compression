CREATE FUNCTION dict_compression_handler(INTERNAL)
RETURNS compression_am_handler
AS 'pg_dict_compression', 'dict_compression_handler'
LANGUAGE C STRICT;

CREATE ACCESS METHOD pg_dict_compression TYPE COMPRESSION
	HANDLER dict_compression_handler;
