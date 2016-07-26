#! /usr/bin/env bash
$XGETTEXT `find . -name \*.qml` -L Java -o $podir/libkirigamiplugin.pot
rm -f rc.cpp

