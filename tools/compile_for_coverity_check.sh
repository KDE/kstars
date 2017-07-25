#!/bin/bash

if [ $# -ne 2 ] ; then
  echo Usage: compile_for_coverity_check.sh email project_token
  echo
  echo email - Email address
  echo project_token - The project token in Coverity Scan service
  echo
  exit 1
fi

# Generate the coverity file
rm -rf cov-int
mkdir cov-int
cmake -Bcov-int -H.. -DCCACHE_SUPPORT=OFF -DUNITY_BUILD=OFF -DCMAKE_BUILD_TYPE=Debug
cov-build --dir cov-int/ make -j4 -C cov-int
if [ $? != 0 ]; then
  echo -e "FATAL: Coverity Scan compilation failed"
  exit 1
fi
nohup find cov-int -type f -print -exec strip --strip-all {} \;
tar -cf - cov-int/ | xz -9 -c - > kstars.tar.xz
# Upload the analysis result to Coverity Scan service
curl --form token=$2 \
  --form email=$1 \
  --form file=@kstars.tar.xz \
  --form version="$(date -d now +%F\ %T\ )(git: $(git reflog -n 1 | cut -d ' ' -f 1))" \
  --form description="Jenkins CI Upload" \
  https://scan.coverity.com/builds?project=kstars_kde

