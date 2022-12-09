lib_LIBRARIES = libdl.a
libdl_a_SOURCES = src/libdl/dl_syscalls.h src/libdl/dl_syscalls.c
libjobsignaler_a_CXXFLAGS = -O2

bin_PROGRAMS = application
applicationdir = src/application
application_SOURCES = src/application/rt-bench.c src/application/rt-bench_utils.c src/application/rt-bench_args.c 
application_HEADERS = src/libdl/dl_syscalls.h src/application/rt-bench.h src/application/rt-bench_utils.h src/application/rt-bench_args.h src/application/rt-bench_types.h
application_LDADD = libdl.a -lpthread -lrt -lm -ljson-c
include_HEADERS = src/libdl/dl_syscalls.h
