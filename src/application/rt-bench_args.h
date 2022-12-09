#ifndef _RT_BENCH_ARGS_H_
#define _RT_BENCH_ARGS_H_

/* for CPU_SET macro */

#include <dl_syscalls.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <json-c/json.h>
#include <sys/types.h>
#include <fcntl.h>


#include "rt-bench_types.h"
#include "rt-bench_utils.h"

#define DEFAULT_THREAD_PRIORITY 10
#define PATH_LENGTH 256

void usage(const char* msg, int ex_code);
void parse_command_line(int argc, char **argv, rtbench_options_t *opts);
void parse_config(const char *filename, rtbench_options_t *opts);

#endif /* _RT_BENCH_ARGS_H_ */
