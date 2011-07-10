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
KSTARS_NOMAD_MYSQL_DB_TO_BIN=./nomadmysql2bin
KSTARS_MYSQL_DB_DB=$(KSTARS_MYSQL_DB_USER)
KSTARS_MYSQL_DB_TBL=tycho2
KSTARS_NOMAD_MYSQL_DB_TBL=nomad

all: mysql2bin binfiletester nomadbinfiletester nomadmysql2bin-merge nomadmysql2bin-split

mysql2bin: mysql2bin.c
	$(CC) $(CFLAGS) `$(MYSQL_CONFIG) --cflags` $@.c $(LDFLAGS) `$(MYSQL_CONFIG) --libs` -o $@

nomadmysql2bin: nomadmysql2bin.c
	$(CC) $(CFLAGS) `$(MYSQL_CONFIG) --cflags` $@.c $(LDFLAGS) `$(MYSQL_CONFIG) --libs` -o $@

nomadbinfile2mysql: nomadbinfile2mysql.cpp
	$(CXX) -D_FILE_OFFSET_BITS=64 $(CXXFLAGS) -I../../htmesh `$(MYSQL_CONFIG) --cflags` $@.cpp $(LDFLAGS) `$(MYSQL_CONFIG) --libs` $(KDEINSTALLDIR)/lib/libhtmesh.a -o $@

nomadmysql2bin-split: nomadmysql2bin-split.c
	$(CC) -D_FILE_OFFSET_BITS=64 $(CFLAGS) `$(MYSQL_CONFIG) --cflags` $@.c $(LDFLAGS) `$(MYSQL_CONFIG) --libs` -o $@

nomadmysql2bin-merge: nomadmysql2bin-merge.c
	$(CC) -D_FILE_OFFSET_BITS=64 $(CFLAGS) $@.c $(LDFLAGS) -lm -o $@

binfiletester: binfiletester.c
	$(CC) $(CFLAGS) $@.c $(LDFLAGS) -lm -o $@

nomadbinfiletester: nomadbinfiletester.c
	$(CC) -D_FILE_OFFSET_BITS=64 $(CFLAGS) $@.c $(LDFLAGS) -lm -o $@

readnomadbindump: readnomadbindump.c
	$(CC) -D_FILE_OFFSET_BITS=64 $(CFLAGS) $@.c $(LDFLAGS) -lm -o $@

clean:
	-rm binfiletester mysql2bin nomadmysql2bin nomadmysql2bin-split nomadbinfiletester readnomadbindump
	-rm ushf usdf nshf nsdf nf dsdf dshf

datafiles: mysql2bin
	echo "If this step hangs, please reduce the value of MYSQL_STARS_PER_QUERY in mysql2bin.c and try again."
	./mysql2bin $(KSTARS_MYSQL_DB_USER) $(KSTARS_MYSQL_DB_PASS) $(KSTARS_MYSQL_DB_DB) usdf ushf dsdf dshf nsdf nshf nf Henry-Draper.idx $(KSTARS_MYSQL_DB_TBL)
	cat ushf usdf > ../unnamedstars.dat
	cat nshf nsdf > ../namedstars.dat
	cat dshf dsdf > ../deepstars.dat
	cat nf > ../starnames.dat
	rm ushf usdf nshf nsdf nf dsdf dshf

nomaddatafiles: nomadmysql2bin
	echo "If this step hangs, please reduce the value of MYSQL_STARS_PER_QUERY in nomadmysql2bin.c and try again."
	./nomadmysql2bin $(KSTARS_MYSQL_DB_USER) $(KSTARS_MYSQL_DB_PASS) usdf ushf $(KSTARS_MYSQL_DB_DB) $(KSTARS_NOMAD_MYSQL_DB_TBL)
	cat ushf usdf > USNO-NOMAD-1e8.dat    # TODO: Change
	rm ushf usdf

install:
# Install nothing
