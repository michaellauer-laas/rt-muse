#include "rt-bench_args.h"

#define PFX "[json] "

/* MACROS */

/* redefine foreach as in <json/json_object.h> but to be ANSI compatible */
#define foreach(obj, entry, key, val, idx) \
  for (({ idx = 0; entry = json_object_get_object(obj)->head;}); \
  ({ if (entry) \
  { key = (char*)entry->k; val = (struct json_object*)entry->v; }; \
  entry; }); \
  ({ entry = entry->next; idx++; }))   
#define set_default_if_needed(key, value, have_def, def_value) do { \
  if (!value) { \
    if (have_def) { \
      log_info(PFX "key: %s <default> %d", key, def_value); \
      return def_value; \
    } else { \
      log_critical(PFX "Key %s not found", key); \
      exit(EXIT_FAILURE); \
    } \
  } \
} while(0)  
#define set_default_if_needed_str(key, value, have_def, def_value) do { \
  if (!value) { \
    if (have_def) { \
      if (!def_value) { \
        log_info(PFX "key: %s <default> NULL", key); \
        return NULL; \
      } \
      log_info(PFX "key: %s <default> %s", \
          key, def_value); \
      return strdup(def_value); \
    } else { \
      log_critical(PFX "Key %s not found", key);  \
      exit(EXIT_FAILURE); \
    } \
  } \
} while (0)

/* END MACROS */

void usage(const char* msg, int ex_code) {
  printf("usage: rt-app <taskset.json>\n");

  if (msg != NULL)
    printf("%s\n", msg);
  exit(ex_code);
}

static inline void assure_type_is(struct json_object *obj,
  struct json_object *parent, const char *key, enum json_type type) {
  if (!json_object_is_type(obj, type)) {
    log_critical("Invalid type for key %s", key);
    log_critical("%s", json_object_to_json_string(parent));
    exit(EXIT_FAILURE);
  }
}

static inline struct json_object* get_in_object(struct json_object *where, 
  const char *what, int nullable) {
  struct json_object *to; 
  if (!nullable && json_object_object_get_ex(where, what, &to))
  {
      log_critical(PFX "Error while parsing config:\n");
      // TODO: fix tokener error managment
     //   "%s", json_tokener_errors[-(unsigned long)to]);
    exit(EXIT_FAILURE);
  }
  if (!nullable && strcmp(json_object_to_json_string(to), "null") == 0) {
    log_critical(PFX "Cannot find key %s", what);
    exit(EXIT_FAILURE);
  }
  return to;
}

static inline int get_bool_value_from(struct json_object *where,
  const char *key, int have_def, int def_value) {

  struct json_object *value;
  int b_value;
  value = get_in_object(where, key, have_def);
  set_default_if_needed(key, value, have_def, def_value);
  assure_type_is(value, where, key, json_type_boolean);
  b_value = json_object_get_boolean(value);
  json_object_put(value);
  return b_value;
}

static inline char* get_string_value_from(struct json_object *where,
  const char *key, int have_def, const char *def_value) {

  struct json_object *value;
  char *s_value;
  value = get_in_object(where, key, have_def);
  set_default_if_needed_str(key, value, have_def, def_value);
  if (json_object_is_type(value, json_type_null))
    return NULL;

  assure_type_is(value, where, key, json_type_string);
  s_value = strdup(json_object_get_string(value));
  json_object_put(value);
  return s_value;
}

static inline int get_int_value_from(struct json_object *where,
  const char *key, int have_def, int def_value) {

  struct json_object *value;
  int i_value;
  value = get_in_object(where, key, have_def);
  set_default_if_needed(key, value, have_def, def_value);
  assure_type_is(value, where, key, json_type_int);
  i_value = json_object_get_int(value);
  json_object_put(value);
  return i_value;
}

static void parse_shared(struct json_object *shared, rtbench_options_t *opts) {
  int i;
  int res = json_object_get_int(shared);
  opts->shared = malloc(sizeof(double) * res);
  pthread_mutexattr_init(&opts->buffermtx_attr);
  pthread_mutex_init(&opts->buffermtx, &opts->buffermtx_attr);
  opts->nshared = res;
}

static void parse_resources(struct json_object *resources, rtbench_options_t *opts) {
  int i;
  int res = json_object_get_int(resources);
  opts->resources = malloc(sizeof(rtbench_resource_t) * res);
  for (i = 0; i < res; i++) {
    pthread_mutexattr_init(&opts->resources[i].mtx_attr);
    pthread_mutex_init(&opts->resources[i].mtx, &opts->resources[i].mtx_attr);
  }
  opts->nresources = res;
}

static void parse_thread_phases(struct json_object *task_phases, thread_data_t *data,
  const rtbench_options_t *opts) {

  /* used in the foreach macro */
  struct lh_entry *entry; char *key; struct json_object *val; int idx;
  int i,j, cur_res_idx, usage_usec;
  struct json_object *phase, *duration, *resource_id;
  int res_dur;
  phase_t ph;

  rtbench_tasks_phase_list_t *tmp, *head, *last;
  char debug_msg[512], tmpmsg[512];

  data->nphases = 0;
  foreach (task_phases, entry, key, val, idx) {
    data->nphases++;
  }
  data->phases = malloc(sizeof(rtbench_tasks_phase_list_t) * data->nphases);

  foreach (task_phases, entry, key, val, idx) {
    data->phases[idx].index = idx;
    string_to_phase(key, &ph);
    data->phases[idx].phase_type = ph;

    switch (ph) {
    case LOCK:
      data->phases[idx].do_phase = lock;
      break;
    case SLEEP:
      data->phases[idx].do_phase = sleep_for;
      break;
    case COMPUTE:
      data->phases[idx].do_phase = compute;
      break;
    case MEMORY:
      data->phases[idx].do_phase = memory;
      break;
    case SHARED:
      data->phases[idx].do_phase = shared;
      break;
    case USER:
      data->phases[idx].do_phase = user;
      break;
    }

    phase = get_in_object(task_phases, key, FALSE);
    // Set loops/duration
    if (ph == USER) ; // do nothing
    else if (ph == SLEEP) {
      duration = get_in_object(phase, "duration", FALSE);
      data->phases[idx].usage = usec_to_timespec(get_int_value_from(phase, "duration", FALSE, 0));
    }
    else {
      duration = get_in_object(phase, "loops", FALSE);
      data->phases[idx].usage = usec_to_timespec(get_int_value_from(phase, "loops", FALSE, 0));
    }

    // Set resource id or memory 
    if (ph == LOCK) { /* if lock, find resource id */
      resource_id = get_in_object(phase, "res", FALSE);
      data->phases[idx].resource_id = get_int_value_from(phase, "res", FALSE, 0);
    }
    else if (ph == MEMORY) {
    	resource_id = get_in_object(phase, "memory", FALSE);
      data->phases[idx].resource_id = get_int_value_from(phase, "memory", FALSE, 0);
    }
    else
      data->phases[idx].resource_id = -1; /* Set to -1 if not lock or memory phase */

  }
}

static void parse_thread_data(char *name, struct json_object *obj,
  int idx, thread_data_t *data, const rtbench_options_t *opts) {

  long exec, period, dline;
  char *policy;
  char def_policy[RTBENCH_POLICY_DESCR_LENGTH];
  struct array_list *cpuset;
  struct json_object *cpuset_obj, *cpu, *phases;
  int i, cpu_idx;

  data->ind = idx;
  data->name = strdup(name);
  data->lock_pages = opts->lock_pages;
  data->sched_prio = DEFAULT_THREAD_PRIORITY;
  data->cpuset = NULL;
  data->cpuset_str = NULL;
  data->sched_flags = 0;

  /* period */
  period = get_int_value_from(obj, "period", TRUE, 10000);
  if (period <= 0) {
    log_critical(PFX "Cannot set negative period");
    exit(EXIT_FAILURE);
  }
  data->period = usec_to_timespec(period);

  /* exec time */
  exec = get_int_value_from(obj, "budget", TRUE, 5000);
  if (exec > period) {
    log_critical(PFX "Budget must be greather than period");
    exit(EXIT_FAILURE);
  }
  if (exec < 0) {
    log_critical(PFX "Cannot set negative exec time");
    exit(EXIT_FAILURE);
  }
  data->min_et = usec_to_timespec(exec);
  data->max_et = usec_to_timespec(exec);

  /* policy */
  policy_to_string(opts->policy, def_policy);
  policy = get_string_value_from(obj, "policy", TRUE, def_policy);
  if (policy) {
    if (string_to_policy(policy, &data->sched_policy) != 0) {
      log_critical(PFX "Invalid policy %s", policy);
      exit(EXIT_FAILURE);
    }
  }
  policy_to_string(data->sched_policy, data->sched_policy_descr);

  /* priority */
  data->sched_prio = get_int_value_from(obj, "priority", TRUE, DEFAULT_THREAD_PRIORITY);
  
  /* deadline */
  dline = get_int_value_from(obj, "deadline", TRUE, period);
  if (dline < exec) {
    log_critical(PFX "Deadline cannot be less than exec time");
    exit(EXIT_FAILURE);
  }
  if (dline > period) {
    log_critical(PFX "Deadline cannot be greater than period");
    exit(EXIT_FAILURE);
  }
  data->deadline = usec_to_timespec(dline);

  /* reservation type */
  if (!get_bool_value_from(obj, "hard_rsv", TRUE, 1))
    data->sched_flags |= SCHED_FLAG_SOFT_RSV;
  
  /* cpu set */
  cpuset_obj = get_in_object(obj, "cpus", TRUE);
  if (cpuset_obj) {
    assure_type_is(cpuset_obj, obj, "cpus", json_type_array);
    data->cpuset_str = (char *) json_object_to_json_string(cpuset_obj);
    data->cpuset = malloc(sizeof(cpu_set_t));
    cpuset = json_object_get_array(cpuset_obj);
    CPU_ZERO(data->cpuset);
    for (i=0; i < json_object_array_length(cpuset_obj); i++) {
      cpu = json_object_array_get_idx(cpuset_obj, i);
      cpu_idx = json_object_get_int(cpu);
      CPU_SET(cpu_idx, data->cpuset);
    }
  } else {
    data->cpuset_str = strdup("-");
    data->cpuset = NULL;
  }

  /* phases */
  phases = get_in_object(obj, "phases", TRUE);
  if (phases) {
    assure_type_is(phases, obj, "phases", json_type_object);
    parse_thread_phases(phases, data, opts);
  }
}

static void parse_tasks(struct json_object *tasks, rtbench_options_t *opts) {
  /* used in the foreach macro */
  struct lh_entry *entry; char *key; struct json_object *val; int idx;

  opts->nthreads = 0;
  foreach(tasks, entry, key, val, idx) { opts->nthreads++; }

  opts->threads_data = malloc(sizeof(thread_data_t) * opts->nthreads);
  foreach (tasks, entry, key, val, idx) {
    parse_thread_data(key, val, idx, &opts->threads_data[idx], opts);
  }
}

static void parse_global(struct json_object *global, rtbench_options_t *opts) {
  char *policy;
  opts->duration = get_int_value_from(global, "duration", TRUE, 1);
  policy = get_string_value_from(global, "default_policy", TRUE, "SCHED_OTHER");
  if (string_to_policy(policy, &opts->policy) != 0) {
    log_critical(PFX "Invalid policy %s", policy);
    exit(EXIT_FAILURE);
  }
  opts->logdir = get_string_value_from(global, "logdir", TRUE, "./");
  opts->logbasename = get_string_value_from(global, "logbasename", TRUE, "rt-bench.log");
  opts->lock_pages = get_bool_value_from(global, "lock_pages", TRUE, 1);
  opts->ftrace = get_bool_value_from(global, "ftrace", TRUE, 1);
}

static void get_opts_from_json_object(struct json_object *root, rtbench_options_t *opts) {
  struct json_object *global, *tasks, *resources, *shared;

  if (root == NULL) {
      log_error(PFX "Error while parsing input JSON: %s");
        // TODO fix json_tokener error management
         //     ,json_tokener_errors[-(unsigned long)root]);
    exit(EXIT_FAILURE);
  }

  global = get_in_object(root, "global", FALSE);  
  tasks = get_in_object(root, "threads", FALSE);  
  resources = get_in_object(root, "resources", FALSE);
  shared = get_in_object(root, "shared", FALSE);

  parse_global(global, opts);
  parse_tasks(tasks, opts); 
  parse_resources(resources, opts);
  parse_shared(shared, opts);
}

void parse_config(const char *filename, rtbench_options_t *opts) {
  int done;
  char *fn = strdup(filename);
  struct json_object *js;
  js = json_object_from_file(fn);
  get_opts_from_json_object(js, opts);
  return;
}

void parse_command_line(int argc, char **argv, rtbench_options_t *opts) {
  if (argc < 2)
    usage(NULL, EXIT_SUCCESS);

  struct stat config_file_stat;
  if (stat(argv[1], &config_file_stat) == 0) {
    parse_config(argv[1], opts);
    return;
  }
  else 
    exit(EXIT_FAILURE);
}
