#!/bin/bash
# Usage: visualize-trixel.sh <USNO NOMAD Data File> <Trixel ID>

TEMPFILE='/tmp/nomad-trixel-dump'
NOMADBINFILETESTER='./nomadbinfiletester'

$NOMADBINFILETESTER $1 $2 &> /tmp/nomad-trixel-dump
grep -E 'RA = |Dec = ' $TEMPFILE | sed ':a;N;s/\s*RA = \([\.0-9]*\)\s*\n\s*Dec = \([+-][\.0-9]*\)/\1 \2/' > "${TEMPFILE}-plot"
gnuplot -p -e "plot \"${TEMPFILE}-plot\" title \"Trixel $2\""

