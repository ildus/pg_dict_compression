EXTENSION = pg_dict_compression
MODULE_big = $(EXTENSION)
OBJS = $(EXTENSION).o $(WIN32RES)

EXTVERSION = 1.0
DATA = $(EXTENSION)--$(EXTVERSION).sql
PGFILEDESC = "dictionary compression method"

REGRESS = basic special

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
