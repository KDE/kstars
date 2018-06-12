#!/bin/bash

echo NOTE: This script must be run in the source root directory

CPPCHECK_ARGS="--force --enable=all --inline-suppr -j 8 -q --error-exitcode=1 -iandroid -ibuild -ikstars/libtess -ikstars/htmesh -ikstars/data/tools -ikstars/ekos/guide/internalguide -ikf5 -isep -UQT_NAMESPACE ."
cppcheck $CPPCHECK_ARGS
