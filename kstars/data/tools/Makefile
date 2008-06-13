#
# Makefile to build data file test utilities
#
# Build the target tools to build programs used to test data files
#
# Begin Date: 2nd July 2008
# Author: Akarsh Simha <akarshsimha@gmail.com>
# License: GPL
#

MYSQL_CONFIG=/usr/bin/mysql_config
KSTARS_MYSQL_DB_TO_BIN=./mysql2bin
KSTARS_MYSQL_DB_DB=$(KSTARS_MYSQL_DB_USER)
KSTARS_MYSQL_DB_TBL=tycho

all: mysql2bin binfiletester

mysql2bin: mysql2bin.c
	$(CC) $(CFLAGS) `$(MYSQL_CONFIG) --cflags` $@.c $(LDFLAGS) `$(MYSQL_CONFIG) --libs` -o $@

binfiletester: binfiletester.c
	$(CC) $(CFLAGS) $@.c $(LDFLAGS) -o $@

clean:
	-rm binfiletester mysql2bin
datafiles: mysql2bin
	echo "If this step hangs, please reduce the value of MYSQL_STARS_PER_QUERY in mysql2bin.c and try again."
	./mysql2bin $(KSTARS_MYSQL_DB_USER) $(KSTARS_MYSQL_DB_PASS) usdf ushf nsdf nshf nf $(KSTARS_MYSQL_DB_DB) $(KSTARS_MYSQL_DB_TBL)
	cat ushf usdf > ../deepstars.dat
	cat nshf nsdf > ../shallowstars.dat
	cat nf > ../starnames.dat
	rm ushf usdf nshf nsdf nf
install:
# Install nothing
