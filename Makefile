MODULE_big = kll_sketch
EXTENSION = kll_sketch
DATA = kll_sketch--1.0.sql
DOCS = README.md
REGRESS := quantile
PG_USER = postgres
REGRESS_OPTS := \
	--load-extension=$(EXTENSION) \
	--user=$(PG_USER) \
	--inputdir=test \
	--outputdir=test \
	--temp-instance=${PWD}/tmpdb

SRCS = quantile.c $(wildcard kll/*.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
