#!/bin/bash

if [ $# -lt 1 ] ; then
  echo Usage: convert_translations.sh install_dir
  echo
  echo The script finds the downloaded translation files \(\*.po\) from the po subdirectory and it converts to
  echo mo format what can be loaded on Android directly.
  echo
  exit 1
fi

mkdir -p $1/locale
DIRS=$(ls -1 po)

echo Convert translations...
for dir in $DIRS; do
  NEW_DIR=$1/locale/$dir/LC_MESSAGES
  mkdir -p $NEW_DIR
  echo Convert po/$dir/kstars.po to $NEW_DIR/kstars.mo
  msgfmt po/$dir/kstars.po -o $NEW_DIR/kstars.mo
done
