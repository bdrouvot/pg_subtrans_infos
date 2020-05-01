MODULE_big = pg_subtrans_infos
OBJS = pg_subtrans_infos.o $(WIN32RES)

EXTENSION = pg_subtrans_infos
DATA = pg_subtrans_infos--1.0.sql
PGFILEDESC = "pg_subtrans_infos"

LDFLAGS_SL += $(filter -lm, $(LIBS))

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
