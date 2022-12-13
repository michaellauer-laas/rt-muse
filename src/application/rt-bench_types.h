#ifndef _RT_BENCH_TYPES_H_
#define _RT_BENCH_TYPES_H_

#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <dl_syscalls.h>

#define RTBENCH_POLICY_DESCR_LENGTH 16

/* exit codes */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* boolean */
#define FALSE 0
#define TRUE  1

typedef struct _ftrace_data_t {
  char *debugfs;
  int trace_fd;
  int marker_fd;
} ftrace_data_t;

typedef enum policy_t { 
  other = SCHED_OTHER, 
  rr = SCHED_RR, 
  fifo = SCHED_FIFO,
  deadline = SCHED_DEADLINE
} policy_t;

/* Shared resources */
typedef struct _rtbench_resource_t {
  pthread_mutex_t mtx;
  pthread_mutexattr_t mtx_attr;
} rtbench_resource_t;

/* Task phases */
typedef enum phase_t { 
  COMPUTE,
  LOCK,
  SLEEP,
  MEMORY,
  SHARED,
  USER
} phase_t;

typedef struct _rtbench_tasks_phase_list_t {
  int index;
  phase_t phase_type;
  struct timespec usage;
  int resource_id;
  void (*do_phase) (int ind, ...);
} rtbench_tasks_phase_list_t;

typedef struct _thread_data_t {
  int ind;
  char *name;
  int lock_pages;
  int duration;
  cpu_set_t *cpuset;
  char *cpuset_str;
  struct timespec min_et, max_et;
  struct timespec period, deadline;
  struct timespec main_app_start;
  FILE *log_handler;
  policy_t sched_policy;
  char sched_policy_descr[RTBENCH_POLICY_DESCR_LENGTH];
  int sched_prio;
  unsigned long sched_flags;
  rtbench_tasks_phase_list_t *phases;
  int nphases;
  struct sched_attr dl_params;
} thread_data_t;

typedef struct _rtbench_options_t {
  int lock_pages;
  thread_data_t *threads_data;
  int nthreads;
  policy_t policy;
  int duration;  
  char *logdir;
  char *logbasename;  
  rtbench_resource_t *resources;
  int nresources;
  double *shared;
  int nshared;
  pthread_mutex_t buffermtx;
  pthread_mutexattr_t buffermtx_attr;
  int ftrace;
} rtbench_options_t;

typedef struct _timing_point_t {
  int ind;
  unsigned long period;
  unsigned long min_et;
  unsigned long max_et;
  unsigned long rel_start_time;
  unsigned long abs_start_time;
  unsigned long end_time;
  unsigned long deadline;
  unsigned long duration;
  unsigned long resp_time;
  long slack;
} timing_point_t;

#endif /* _RT_BENCH_TYPES_H_ */
