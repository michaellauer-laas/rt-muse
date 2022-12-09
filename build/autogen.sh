#!/bin/sh

################################################################################
# Avoid remembering all the automake toolchain, make will call this script
################################################################################

# create new build
mkdir bin

# copy source files
cd bin
cp -r ../src .
cp src/*/*.h .
cp ../build/configure.ac .
cp ../build/Makefile.am .

# using autotools
autoreconf --force --install
#export CFLAGS="-O0 -DTRACE_BEGINS_SLEEP -DTRACE_BEGINS_COMPUTE -DTRACE_BEGINS_LOCK -DTRACE_LOCK_ACQUIRED -DTRACE_SETS_ZERO_TIME -DTRACE_BEGINS_LOOP -DTRACE_END_LOOP"
export CFLAGS="-O0 -DTRACE_SETS_ZERO_TIME -DTRACE_BEGINS_LOOP -DNO_LOGS -D_GNU_SOURCE"
./configure
make

# made
cd ..
