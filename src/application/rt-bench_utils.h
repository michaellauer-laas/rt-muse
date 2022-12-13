#ifndef _RT_BENCH_UTILS_H_
#define _RT_BENCH_UTILS_H_

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rt-bench_types.h"

#ifndef LOG_PREFIX
#define LOG_PREFIX "[rt-bench] "
#endif
#ifndef LOG_LEVEL
#define LOG_LEVEL 100
#endif

#define LOG_LEVEL_DEBUG 100
#define LOG_LEVEL_INFO 75
#define LOG_LEVEL_NOTICE 50
#define LOG_LEVEL_ERROR 10
#define LOG_LEVEL_CRITICAL 10

#define BUF_SIZE 100

#define rtbench_log_to(where, level, level_pfx, msg, args...) \
do { \
    if (level <= LOG_LEVEL) { \
        fprintf(where, LOG_PREFIX level_pfx msg "\n", ##args); \
    } \
} while (0);
#define log_ftrace(mark_fd, msg, args...) \
do {                  \
    ftrace_write(mark_fd, msg, ##args); \
} while (0);
#ifdef NO_LOGS
#define log_notice(msg, args...)
#define log_info(msg, args...)
#define log_debug(msg, args...)
#define log_error(msg, args...)
#define log_critical(msg, args...)
#else
#define log_notice(msg, args...) \
do { \
    rtbench_log_to(stderr, LOG_LEVEL_NOTICE, "<notice> ", msg, ##args); \
} while (0);
#define log_info(msg, args...) \
do { \
    rtbench_log_to(stderr, LOG_LEVEL_INFO, "<info> ", msg, ##args); \
} while (0);
#define log_debug(msg, args...) \
do { \
    rtbench_log_to(stderr, LOG_LEVEL_DEBUG, "<debug> ", msg, ##args); \
} while (0);
#define log_error(msg, args...) \
do { \
    rtbench_log_to(stderr, LOG_LEVEL_ERROR, "<error> ", msg, ##args); \
} while (0);
#define log_critical(msg, args...) \
do { \
    rtbench_log_to(stderr, LOG_LEVEL_CRITICAL, "<crit> ", msg, ##args); \
} while (0);
#endif
void log_timing(FILE *handler, timing_point_t *t);
void ftrace_write(int mark_fd, const char *fmt, ...);

struct timespec usec_to_timespec(unsigned long usec);
unsigned long timespec_to_usec(struct timespec *ts);
__u64 timespec_to_nsec(struct timespec *ts);
long timespec_to_lusec(struct timespec *ts);
struct timespec msec_to_timespec(unsigned int msec);
unsigned int timespec_to_msec(struct timespec *ts);
struct timespec timespec_add(struct timespec *t1, struct timespec *t2);
struct timespec timespec_sub(struct timespec *t1, struct timespec *t2);
int timespec_lower(struct timespec *what, struct timespec *than);

int string_to_policy(const char *policy_name, policy_t *policy);
int policy_to_string(policy_t policy, char *policy_name);
int string_to_phase(const char *phase_name, phase_t *phase);

pid_t gettid(void);

void sleep_for (int ind, ...);
void compute (int ind, ...);
void lock (int ind, ...);
void memory (int ind, ...);
void shared (int ind, ...);
void user (int ind, ...);

#define exec_phase(data, pn) \
do { \
    data->phases[pn].do_phase(data->ind, &data->phases[pn].usage, \
            data->phases[pn].resource_id,  NULL, 0); \
} while (0);

#endif /* _RT_BENCH_UTILS_H_ */
