#!/bin/sh

width=32
halfwidth=$((width/2))
dir="../"

for file in *.svg
do
     filename="${file%.*}"
     /usr/bin/inkscape -z -f "${file}" -w $width -e "${dir}/${filename}.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*2)) -e "${dir}/${filename}@2x.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*4)) -e "${dir}/${filename}@3x.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*6)) -e "${dir}/${filename}@4x.png"
done