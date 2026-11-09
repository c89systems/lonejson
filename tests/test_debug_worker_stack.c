#define _GNU_SOURCE 1
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SMALL_STACK (128u * 1024u)
#define EXPLICIT_STACK (256u * 1024u)

typedef struct observation {
  size_t size;
  size_t guard;
  int nested;
  int check_guard;
  int failed;
} observation;

static int wait_success(pid_t child) {
  int status;
  return child > 0 && waitpid(child, &status, 0) == child &&
         WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int guard_faults(void *base) {
  pid_t child;
  int status;
  child = fork();
  if (child == 0) {
    struct rlimit no_core;
    no_core.rlim_cur = no_core.rlim_max = 0;
    if (setrlimit(RLIMIT_CORE, &no_core) != 0)
      _exit(2);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    /* Probe the guard outside the usable, pthread-owned stack. This also
     * detects lightweight guards that need not appear as PROT_NONE mappings. */
    *((volatile unsigned char *)base - 1) = 0;
    _exit(0);
  }
  return child > 0 && waitpid(child, &status, 0) == child &&
         WIFSIGNALED(status) &&
         (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS);
}

static void *observe(void *user) {
  observation *result = (observation *)user;
  pthread_attr_t attributes;
  void *base;
  if (pthread_getattr_np(pthread_self(), &attributes) != 0) {
    result->failed = 1;
    return NULL;
  }
  result->failed =
      pthread_attr_getstack(&attributes, &base, &result->size) != 0 ||
      pthread_attr_getguardsize(&attributes, &result->guard) != 0;
  pthread_attr_destroy(&attributes);
  if (!result->failed && result->check_guard &&
      (result->guard == 0u || !guard_faults(base))) {
    fprintf(stderr, "worker guard access did not fault\n");
    result->failed = 1;
  }
  if (result->nested && !result->failed) {
    observation nested;
    pthread_t thread;
    memset(&nested, 0, sizeof(nested));
    nested.check_guard = 1;
    if (pthread_create(&thread, NULL, observe, &nested) != 0 ||
        pthread_join(thread, NULL) != 0 || nested.failed ||
        nested.size != SMALL_STACK) {
      fprintf(stderr, "nested default worker: %lu bytes, expected %u\n",
              (unsigned long)nested.size, SMALL_STACK);
      result->failed = 1;
    }
  }
  return NULL;
}

static int create_worker(const pthread_attr_t *attributes,
                         observation *result) {
  pthread_t thread;
  return pthread_create(&thread, attributes, observe, result) == 0 &&
         pthread_join(thread, NULL) == 0 && !result->failed;
}

static int check_workers(void) {
  observation explicit_worker;
  observation default_worker;
  pthread_attr_t attributes;
  long page = sysconf(_SC_PAGESIZE);
  int okay;
  size_t declared_size;
  if (pthread_getattr_default_np(&attributes) != 0)
    return 0;
  okay = pthread_attr_getstacksize(&attributes, &declared_size) == 0;
  pthread_attr_destroy(&attributes);
  if (!okay || declared_size != SMALL_STACK)
    return 0;
  memset(&explicit_worker, 0, sizeof(explicit_worker));
  memset(&default_worker, 0, sizeof(default_worker));
  explicit_worker.check_guard = 1;
  default_worker.check_guard = default_worker.nested = 1;
  if (page <= 0 || pthread_attr_init(&attributes) != 0)
    return 0;
  if (pthread_attr_setstacksize(&attributes, EXPLICIT_STACK) != 0 ||
      pthread_attr_setguardsize(&attributes, (size_t)page * 2u) != 0) {
    pthread_attr_destroy(&attributes);
    return 0;
  }
  okay = create_worker(&attributes, &explicit_worker);
  pthread_attr_destroy(&attributes);
  if (!okay || explicit_worker.size != EXPLICIT_STACK ||
      explicit_worker.guard != (size_t)page * 2u) {
    fprintf(stderr, "explicit stack/guard request changed: %lu/%lu\n",
            (unsigned long)explicit_worker.size,
            (unsigned long)explicit_worker.guard);
    return 0;
  }
  /* Joining a 256 KiB worker first would permit larger cached-stack reuse
   * without glibc.pthread.stack_cache_size=0 at process startup. */
  if (!create_worker(NULL, &default_worker) ||
      default_worker.size != SMALL_STACK) {
    fprintf(stderr, "default worker: %lu bytes, expected %u\n",
            (unsigned long)default_worker.size, SMALL_STACK);
    return 0;
  }
  printf("default/nested workers: 128 KiB; explicit: 256 KiB; guards fault\n");
  return 1;
}

static int check_control(const char *control, const struct rlimit *main_limit) {
  int channel[2];
  FILE *output;
  pid_t child;
  unsigned long limit;
  unsigned long maximum;
  unsigned long size;
  unsigned long default_size;
  int okay;
  if (pipe(channel) != 0)
    return 0;
  child = fork();
  if (child == 0) {
    close(channel[0]);
    if (dup2(channel[1], STDOUT_FILENO) < 0)
      _exit(2);
    close(channel[1]);
    execl(control, control, (char *)NULL);
    _exit(2);
  }
  close(channel[1]);
  output = fdopen(channel[0], "r");
  if (output == NULL) {
    close(channel[0]);
    return 0;
  }
  okay = fscanf(output, "%lu %lu %lu %lu", &limit, &maximum, &size,
                &default_size) == 4;
  fclose(output);
  okay = wait_success(child) && okay;
  if (!okay)
    return 0;
  if (limit != (unsigned long)main_limit->rlim_cur ||
      maximum != (unsigned long)main_limit->rlim_max || size != default_size ||
      (limit > SMALL_STACK && size <= SMALL_STACK)) {
    fprintf(stderr, "main limit or build-tool worker defaults changed\n");
    return 0;
  }
  printf("main limit unchanged; unmodified executable worker: %lu bytes\n",
         size);
  return 1;
}

int main(int argc, char **argv) {
  struct rlimit main_limit;
  struct rlimit after;
  pthread_attr_t attributes;
  size_t default_size;
  observation control;
  pid_t child;
  if (getrlimit(RLIMIT_STACK, &main_limit) != 0)
    return 1;
  if (argc == 3 && strcmp(argv[1], "--run") == 0) {
    char limit[32];
    char maximum[32];
    snprintf(limit, sizeof(limit), "%lu", (unsigned long)main_limit.rlim_cur);
    snprintf(maximum, sizeof(maximum), "%lu",
             (unsigned long)main_limit.rlim_max);
    execl(argv[2], argv[2], argv[0], limit, maximum, (char *)NULL);
    return 1;
  }
  if (argc == 4 &&
      ((unsigned long)main_limit.rlim_cur != strtoul(argv[2], NULL, 10) ||
       (unsigned long)main_limit.rlim_max != strtoul(argv[3], NULL, 10))) {
    fprintf(stderr, "Debug startup changed the main thread stack limits\n");
    return 1;
  }
  if (argc == 1) {
    memset(&control, 0, sizeof(control));
    if (pthread_getattr_default_np(&attributes) != 0)
      return 1;
    if (pthread_attr_getstacksize(&attributes, &default_size) != 0)
      return 1;
    pthread_attr_destroy(&attributes);
    if (!create_worker(NULL, &control))
      return 1;
    printf("%lu %lu %lu %lu\n", (unsigned long)main_limit.rlim_cur,
           (unsigned long)main_limit.rlim_max, (unsigned long)control.size,
           (unsigned long)default_size);
    return 0;
  }
  if (!check_workers())
    return 1;
  if (strcmp(argv[1], "--child") == 0)
    return 0;
  if (!check_control(argv[1], &main_limit))
    return 1;
  fflush(stdout);
  child = fork();
  if (child == 0) {
    execl(argv[0], argv[0], "--child", (char *)NULL);
    _exit(2);
  }
  if (!wait_success(child)) {
    fprintf(stderr, "executed child application lost the Debug stack policy\n");
    return 1;
  }
  if (getrlimit(RLIMIT_STACK, &after) != 0 ||
      after.rlim_cur != main_limit.rlim_cur ||
      after.rlim_max != main_limit.rlim_max)
    return 1;
  return 0;
}
