#!/bin/bash

# Get the directory where the script is stored
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$DIR"/kstars
krazy2all --check-sets c++,foss,qt5 --exclude typedefs,cpp,copyright,license,syscalls,postfixop,includes
cd ..
cd "$DIR"/datahandlers
krazy2all --check-sets c++,foss,qt5 --exclude typedefs,cpp,copyright,license,syscalls,postfixop,includes
cd ..
cd "$DIR"/Tests
krazy2all --check-sets c++,foss,qt5 --exclude typedefs,cpp,copyright,license,syscalls,postfixop,includes
cd ..
