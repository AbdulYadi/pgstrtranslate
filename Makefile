MODULE_big = pgstrtranslate
OBJS = pgstrtranslate.o
EXTENSION = pgstrtranslate
DATA = pgstrtranslate--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

include $(PGXS)
