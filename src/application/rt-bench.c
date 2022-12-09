#include "rt-bench.h"
#include "rt-bench_utils.h"
#include "user.h"
#include "math.h"

rtbench_options_t opts;
static int errno;
static int nthreads;
static pthread_t *threads;
static pthread_barrier_t threads_barrier;
static struct timespec t_zero;
static volatile int continue_running;
static ftrace_data_t ft_data = {
  .debugfs = "/sys/kernel/debug",
  .trace_fd = -1,
  .marker_fd = -1,
};

static inline void busywait(struct timespec *to) {
  struct timespec t_step;
  while (1) {
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
    if (!timespec_lower(&t_step, to))
      break;
  }
}

void shared (int ind, ...) {
  int memory_used, loops, i;
  struct timespec *t_spec;
  va_list argp;
  va_start(argp, ind);
  t_spec = va_arg(argp, struct timespec*);
  va_end(argp); 
  loops = timespec_to_usec(t_spec);
  if (opts.nshared != 0) {
	  pthread_mutex_lock(&opts.buffermtx);
	  for (i = 0; i < loops; i++) {
	    opts.shared[i%opts.nshared] += 0.5;
	    opts.shared[i%opts.nshared] -= floor(opts.shared[i%opts.nshared]);
	  }
	  pthread_mutex_unlock(&opts.buffermtx);
  }
}

void memory (int ind, ...) {
  int memory_used, loops, i;
  double *accumulator;
  struct timespec *t_spec;
  va_list argp;
  va_start(argp, ind);
  t_spec = va_arg(argp, struct timespec*);
  memory_used = va_arg(argp, int);
  va_end(argp); 
  loops = timespec_to_usec(t_spec);
  accumulator = malloc(memory_used*sizeof(double));
  for (i = 0; i < loops; i++) {
    accumulator[i%memory_used] += 0.5;
    accumulator[i%memory_used] -= floor(accumulator[i%memory_used]);
  }
  free(accumulator);
}

void sleep_for (int ind, ...) {
  struct timespec *t_sleep, t_now;
  va_list argp;
  va_start(argp, ind);
  t_sleep = va_arg(argp, struct timespec*);
  va_end(argp);
  clock_gettime(CLOCK_MONOTONIC, &t_now);
  t_now = timespec_add(&t_now, t_sleep);
#ifdef TRACE_BEGINS_SLEEP
  log_ftrace(ft_data.marker_fd, "[%d] begins sleep_for", ind+1);
#endif
  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_now, NULL);
}

void compute (int ind, ...) {
  //  unsigned int loops, i, counter = 0;
  unsigned int loops, i;
  double accumulator=0.25;
  struct timespec *t_spec;
  va_list argp;
  va_start(argp, ind);
  t_spec = va_arg(argp, struct timespec*);
  va_end(argp);
  loops = timespec_to_usec(t_spec);
#ifdef TRACE_BEGINS_COMPUTE
  log_ftrace(ft_data.marker_fd, "[%d] begins compute", ind+1);
#endif
  for (i = 0; i < loops; i++) {
    accumulator += 0.5;
    accumulator -= floor(accumulator);
  }
}

void lock(int ind, ...) {
  int resource_id;
  unsigned int loops, i;
  double accumulator=0.25;
  //struct timespec t_start, now, t_exec, t_totexec;
  struct timespec *t_spec;
  va_list argp;
  va_start(argp, ind);
  t_spec = va_arg(argp, struct timespec*);
  resource_id = va_arg(argp, int);
  va_end(argp);
  //clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);
  loops = timespec_to_usec(t_spec);
#ifdef TRACE_BEGINS_LOCK
  log_ftrace(ft_data.marker_fd, "[%d] begins lock", ind+1);
#endif
  pthread_mutex_lock(&opts.resources[resource_id].mtx);
#ifdef TRACE_LOCK_ACQUIRED
  log_ftrace(ft_data.marker_fd, "[%d] lock acquired", ind+1);
#endif
  // clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
  for (i = 0; i < loops; i++) {
    accumulator += 0.5;
    accumulator -= floor(accumulator);
  }
  // counter = (++counter) * i;
  // t_exec = timespec_add(&now, t_spec);
  // busywait(&t_exec);
  pthread_mutex_unlock(&opts.resources[resource_id].mtx);
}

static void shutdown(int sig) {
  int i;
  /* notify threads, join them, then exit */
  continue_running = 0;
  for (i = 0; i < nthreads; i++)
    pthread_join(threads[i], NULL);
  if (opts.ftrace) {
    close(ft_data.trace_fd);
    close(ft_data.marker_fd);
  }
  /* free things that have been allocated */
  for (i = 0; i < nthreads; i++) {
  	free(opts.threads_data[i].cpuset);
  	free(opts.threads_data[i].phases);
  }
  free(opts.resources);
  free(opts.shared);
  free(opts.threads_data);
  free(threads);
  exit(EXIT_SUCCESS);
}

void *thread_body(void *arg) {
  
  int ret;
  int nperiods;
  struct sched_param param;
  timing_point_t *timings;
  pid_t tid;
  struct sched_attr attr;
  unsigned int flags = 0;
  struct timespec t, t_next;
  timing_point_t tmp_timing;
  timing_point_t *curr_timing;
  unsigned long t_start_usec;
  int i = 0;

  thread_data_t *data = (thread_data_t*) arg;

  /* set thread affinity */
  if (data->cpuset != NULL) {
    log_notice("[%d] setting cpu affinity to CPU(s) %s",
      data->ind, data->cpuset_str);
    ret = pthread_setaffinity_np(pthread_self(),
      sizeof(cpu_set_t), data->cpuset);
    if (ret < 0) {
      errno = ret;
      perror("pthread_setaffinity_np");
      exit(EXIT_FAILURE);
    }
  }

  /* set scheduling policy and print pretty info on stdout */
  log_notice("[%d] Using %s policy:", data->ind, data->sched_policy_descr);
  switch (data->sched_policy) {
    case rr:
    case fifo:
      fprintf(data->log_handler, "# Policy : %s\n",
        (data->sched_policy == rr ? "SCHED_RR" : "SCHED_FIFO"));
      param.sched_priority = data->sched_prio;
      ret = pthread_setschedparam(pthread_self(),
        data->sched_policy, &param);
      if (ret != 0) {
        errno = ret; 
        perror("pthread_setschedparam"); 
        exit(EXIT_FAILURE);
      }

      log_notice("[%d] starting thread with period: %" PRIu64 
        ", exec: %" PRIu64 ",""deadline: %" PRIu64 ", priority: %d",
        data->ind,
        timespec_to_usec(&data->period), 
        timespec_to_usec(&data->min_et),
        timespec_to_usec(&data->deadline),
        data->sched_prio
      );
      break;
    case other:
      fprintf(data->log_handler, "# Policy : SCHED_OTHER\n");
      log_notice("[%d] starting thread with period: %" PRIu64 
           ", exec: %" PRIu64 ",""deadline: %" PRIu64 "", data->ind,
        timespec_to_usec(&data->period), 
        timespec_to_usec(&data->min_et),
        timespec_to_usec(&data->deadline)
      );
      data->lock_pages = 0; /* forced off for SCHED_OTHER */
      break;
    case deadline:
      fprintf(data->log_handler, "# Policy : SCHED_DEADLINE\n");
      tid = gettid();
      attr.size = sizeof(attr);
      attr.sched_flags = data->sched_flags;
      if (data->sched_flags && SCHED_FLAG_SOFT_RSV)
        fprintf(data->log_handler, "# Type : SOFT_RSV\n");
      else
        fprintf(data->log_handler, "# Type : HARD_RSV\n");
      attr.sched_policy = SCHED_DEADLINE;
      attr.sched_priority = 0;
      attr.sched_runtime = timespec_to_nsec(&data->max_et) +
        (timespec_to_nsec(&data->max_et) /100) * BUDGET_OVERP;
      attr.sched_deadline = timespec_to_nsec(&data->period);
      attr.sched_period = timespec_to_nsec(&data->period);  
      break;
    default:
      log_error("Unknown scheduling policy %d",
        data->sched_policy);
      exit(EXIT_FAILURE);
  }

  if (data->lock_pages == 1) {
    log_notice("[%d] Locking pages in memory", data->ind);
    ret = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (ret < 0) {
      errno = ret;
      perror("mlockall");
      exit(EXIT_FAILURE);
    }
  }

  /* if we know the duration we can calculate how many periods we will
   * do at most, and the log to memory, instead of logging to file.
   */
  timings = NULL;
  if (data->duration > 0) {
    nperiods = (int) ceil( (data->duration * 10e6) / 
              (double) timespec_to_usec(&data->period));
    timings = malloc ( nperiods * sizeof(timing_point_t));
  }

  fprintf(data->log_handler, "#idx\tperiod\tmin_et\tmax_et\trel_st\tstart"
           "\t\tend\t\tdeadline\tdur.\tslack\tresp_t"
           "\tBudget\tUsed Budget\n");

  if (data->ind == 0) {
    clock_gettime(CLOCK_MONOTONIC, &t_zero);
#ifdef TRACE_SETS_ZERO_TIME
    if (opts.ftrace)
      log_ftrace(ft_data.marker_fd,
           "[%d] sets zero time",
           data->ind);
#endif
  }

  pthread_barrier_wait(&threads_barrier);

  /*
   * Set the task to SCHED_DEADLINE as far as possible touching its
   * budget as little as possible for the first iteration.
   */
  if (data->sched_policy == SCHED_DEADLINE) {
    ret = sched_setattr(tid, &attr, flags);
    if (ret != 0) {
      log_critical("[%d] sched_setattr "
        "returned %d", data->ind, ret);
      errno = ret;
      perror("sched_setattr");
      exit(EXIT_FAILURE);
    }
  }

  t = t_zero;
  t_next = msec_to_timespec(1000LL);
  t_next = timespec_add(&t, &t_next);
  clock_nanosleep(CLOCK_MONOTONIC, 
    TIMER_ABSTIME, 
    &t_next,
    NULL);

  data->deadline = timespec_add(&t_next, &data->deadline);

  while (continue_running) {
    int pn;
    struct timespec t_start, t_end, t_diff, t_slack, t_resp;

    /* Thread numeration reported starts with 1 */
#ifdef TRACE_BEGINS_LOOP
    if (opts.ftrace)
      log_ftrace(ft_data.marker_fd, "[%d] begins job %d", data->ind+1, i);
#endif
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    if (data->nphases == 0) {
      compute(data->ind, &data->min_et, NULL, 0);
    } else {
      for (pn = 0; pn < data->nphases; pn++) {
        log_notice("[%d] phase %d start", data->ind+1, pn);
        exec_phase(data, pn);
        log_notice("[%d] phase %d end", data->ind+1, pn);
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    
    t_diff = timespec_sub(&t_end, &t_start);
    t_slack = timespec_sub(&data->deadline, &t_end);
    t_resp = timespec_sub(&t_end, &t_next);
    t_start_usec = timespec_to_usec(&t_start); 

    if (i < nperiods) {
      if (timings)
        curr_timing = &timings[i];
      else
        curr_timing = &tmp_timing;

      curr_timing->ind = data->ind;
      curr_timing->period = timespec_to_usec(&data->period);
      curr_timing->min_et = timespec_to_usec(&data->min_et);
      curr_timing->max_et = timespec_to_usec(&data->max_et);
      curr_timing->rel_start_time = 
        t_start_usec - timespec_to_usec(&data->main_app_start);
      curr_timing->abs_start_time = t_start_usec;
      curr_timing->end_time = timespec_to_usec(&t_end);
      curr_timing->deadline = timespec_to_usec(&data->deadline);
      curr_timing->duration = timespec_to_usec(&t_diff);
      curr_timing->slack =  timespec_to_lusec(&t_slack);
      curr_timing->resp_time =  timespec_to_usec(&t_resp);
    }
    if (!timings)
      log_timing(data->log_handler, curr_timing);

    t_next = timespec_add(&t_next, &data->period);
    data->deadline = timespec_add(&data->deadline, &data->period);
#ifdef TRACE_END_LOOP
    if (opts.ftrace)
      log_ftrace(ft_data.marker_fd, "[%d] end loop %d", data->ind, i);
#endif
    if (curr_timing->slack < 0)
      log_notice("[%d] DEADLINE MISS !!!", data->ind+1);
    i++;
  }

  free(timings);
}

int main(int argc, char* argv[]) {

  int i;
  struct timespec t_start;
  thread_data_t *tdata;
  char tmp[PATH_LENGTH];

  parse_command_line(argc, argv, &opts);
  nthreads = opts.nthreads;
  threads = malloc(nthreads * sizeof(pthread_t));
  pthread_barrier_init(&threads_barrier, NULL, nthreads);

  /* install signal handlers for proper shutdown */
  signal(SIGQUIT, shutdown);
  signal(SIGTERM, shutdown);
  signal(SIGHUP, shutdown);
  signal(SIGINT, shutdown);

  /* if using ftrace open trace and marker fds */
  if (opts.ftrace) {
    log_notice("configuring ftrace");
    strcpy(tmp, ft_data.debugfs);
    strcat(tmp, "/tracing/tracing_on");
    ft_data.trace_fd = open(tmp, O_WRONLY);
    if (ft_data.trace_fd < 0) {
      log_error("Cannot open trace_fd file %s", tmp);
      exit(EXIT_FAILURE);
    }
    strcpy(tmp, ft_data.debugfs);
    strcat(tmp, "/tracing/trace_marker");
    ft_data.marker_fd = open(tmp, O_WRONLY);
    if (ft_data.trace_fd < 0) {
      log_error("Cannot open trace_marker file %s", tmp);
      exit(EXIT_FAILURE);
    }
    log_ftrace(ft_data.trace_fd, "1");
    log_ftrace(ft_data.marker_fd, "main creates threads\n");
  }

  continue_running = 1;

  /* Take the beginning time for everything */
  clock_gettime(CLOCK_MONOTONIC, &t_start);

  /* start threads */
  for (i = 0; i < nthreads; i++) {
    tdata = &opts.threads_data[i];
    tdata->duration = opts.duration;
    tdata->main_app_start = t_start;
    tdata->lock_pages = opts.lock_pages;
    if (opts.logdir) {
      snprintf(tmp, PATH_LENGTH, "%s/%s-%s.log",
         opts.logdir,
         opts.logbasename,
         tdata->name);
      tdata->log_handler = fopen(tmp, "w");
      if (!tdata->log_handler){
        log_error("Cannot open logfile %s", tmp);
        exit(EXIT_FAILURE);
      }
    } else {
      tdata->log_handler = stdout;
    }

    if (pthread_create(&threads[i],
          NULL, 
          thread_body, 
          (void*) tdata))
      exit(EXIT_FAILURE);
  }

  if (opts.duration > 0) {
    sleep(opts.duration);
    if (opts.ftrace)
      log_notice("main shutdown");
    shutdown(SIGTERM);
  }

  for (i = 0; i < nthreads; i++)  {
    pthread_join(threads[i], NULL);
  }

  if (opts.ftrace) {
    log_notice("stopping ftrace");
    log_notice("main ends\n");
    close(ft_data.trace_fd);
    close(ft_data.marker_fd);
  }

  exit(EXIT_SUCCESS);

}
