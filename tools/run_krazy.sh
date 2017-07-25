#!/bin/bash

echo NOTE: This script must be run in the source root directory

krazy2all --check-sets c++,foss,qt5 --exclude typedefs,cpp,copyright,license,syscalls,postfixop,includes
