#
# Makefile to build data file test utilities
#
# Build the target tools to build programs used to test data files
#
# Begin Date: 2nd July 2008
# Author: Akarsh Simha <akarshsimha@gmail.com>
# License: GPL
#

#                ACHTUNG! DIESES MAKEFILE IST KAPUT.
#
# NOTE: This is a extremely non-portable Makefile, just like the rest
# of the files in this directory. Most likely, it will not work out of
# the box on your computer. Here's what you might need to do:
#
# 1. Ensure you're running GNU/Linux or other POSIX compliant OS. Else
# ask some leet guru how to get this to work on your platform.
# 2. Check if you have mysql_config in the same place as defined in MYSQL_CONFIG
# 3. Define the variables KSTARS_MYSQL_DB_USER and KSTARS_MYSQL_DB_DB
# which specify which user and database you want to use on your
# *local* MySQL server. Everything here assumes localhost. If you
# can't run MySQL off localhost, fix that in all those C programs.
# 4. Ensure you have libhtmesh.a from KStars' build of libhtmesh at
# $KDEINSTALLDIR/lib/libhtmesh.a
#
# TODO:
# 1. Make HTMesh a shared object library
# 2. Consider migrating to CMake and being part of the buildsystem of
# KStars.
#
# My lack of experience and know-how of buildsystems doesn't help
# here. If you can fix this, please do. -- Akarsh
#
# If you need any information, please feel free to contact me at
# <akarsh.simha@kdemail.net> or <akarshsimha@gmail.com>


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
	$(CXX) -D_FILE_OFFSET_BITS=64 $(CXXFLAGS) -I../../htmesh -I../.. `$(MYSQL_CONFIG) --cflags` $@.cpp $(LDFLAGS) `$(MYSQL_CONFIG) --libs` $(KDEINSTALLDIR)/lib/libhtmesh.a -o $@

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

nomadbinfile2sqlite: nomadbinfile2sqlite.cpp
	$(CXX) -D_FILE_OFFSET_BITS=64 $(CXXFLAGS) -l sqlite3 -I../../htmesh -I../..  nomadbinfile2sqlite.cpp $(KDEINSTALLDIR)/lib/libhtmesh.a -o $@
clean:
	-rm binfiletester mysql2bin nomadmysql2bin nomadmysql2bin-split nomadmysql2bin-merge nomadbinfiletester readnomadbindump nomadbinfile2mysql
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
