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

all: mysql2bin binfiletester

mysql2bin: mysql2bin.c
	$(CC) $(CFLAGS) `$(MYSQL_CONFIG) --cflags` $@.c $(LDFLAGS) `$(MYSQL_CONFIG) --libs` -o $@

binfiletester: binfiletester.c
	$(CC) $(CFLAGS) $@.c $(LDFLAGS) -o $@

clean:
	-rm binfiletester mysql2bin

install:
# Install nothing
