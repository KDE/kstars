#!/bin/sh

echo -n "" > "CMakeLists.txt"

echo "install( FILES" >> "CMakeLists.txt"

for file in *.png
do
     echo "\t ${file}" >> "CMakeLists.txt"
done

set -f
echo '\t DESTINATION ${KDE_INSTALL_DATADIR}/kstars/kstarslite/qml/images )' >> "CMakeLists.txt"