#!/bin/bash

# Script to generate data dumps for each trixel
#
# SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

#
# USAGE: <this script> <DB username> <DB password> <file prefix> <DB name> <Table Name> <LOG File>
#

MAX_TRIXEL=32767
NOMADMYSQL2BIN_SPLIT='./nomadmysql2bin-split'
DB_USER=$1
DB_PASS=$2
FILE_PREFIX=$3
DB_NAME=$4
TBL_NAME=$5
LOGFILE=$6

for trixel in `seq 0 $MAX_TRIXEL`; do
    CMD="$NOMADMYSQL2BIN_SPLIT $DB_USER $DB_PASS $FILE_PREFIX$trixel $trixel $DB_NAME $TBL_NAME >> $LOGFILE"
    echo "Executing $CMD" >> $LOGFILE
    $CMD
    status=$?
    if [ $status -ne 0 ] ; then
        echo "***ERROR*** MySQL --> binary dump failed. $NOMADMYSQL2BIN_SPLIT exited with status: $status on trixel $trixel"
        echo "***ERROR*** MySQL --> binary dump failed. $NOMADMYSQL2BIN_SPLIT exited with status: $status on trixel $trixel" >> $LOGFILE
    else
        echo "=================== Trixel $trixel dumped successfully ===================" >> $LOGFILE
    fi;
done;


    