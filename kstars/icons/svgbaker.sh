#!/bin/bash
#
# Generates raster PNG icons from an SVG input using ImageMagick convert
#
# Expects the SVG file as an argument
#
file=$1;

for i in 22 32 48 64; do
    convert -density 1200 -resize ${i}x${i} ${file} `echo ${file} | sed "s/svg/png/;s/^sc/${i}/"`;
done;
