#!/bin/sh

width=32
halfwidth=$((width/2))
dir=".."

for file in *.png
do
	filename="${file%.*}"
	echo $filename
     /usr/bin/inkscape -z -f "${file}" -w $width -e "$dir/ldpi/icons/${filename}.png"
     echo "<file alias=\"${filename}.png\">images/ldpi/icons/${filename}.png</file>" >> $dir/qrc.txt
     /usr/bin/inkscape -z -f "${file}" -w $width -e "$dir/mdpi/icons/${filename}.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth)) -e "$dir/hdpi/icons/${filename}.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*2)) -e "$dir/xhdpi/icons/${filename}.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*4)) -e "$dir/xxhdpi/icons/${filename}.png"
     /usr/bin/inkscape -z -f "${file}" -w $(($width + halfwidth*6)) -e "$dir/xxxhdpi/icons/${filename}.png"
done
