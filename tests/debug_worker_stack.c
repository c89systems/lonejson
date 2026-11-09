/* Private Debug executable setup; never linked into the installed libraries. */
#define _GNU_SOURCE 1
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void __attribute__((constructor)) lonejson_debug_worker_stack(void) {
  pthread_attr_t attributes;
  size_t guard;
  long page;
  int status;

  status = pthread_getattr_default_np(&attributes);
  if (status == 0) {
    status = pthread_attr_setstacksize(&attributes, 128u * 1024u);
    if (status == 0)
      status = pthread_attr_getguardsize(&attributes, &guard);
    if (status == 0 && guard == 0u) {
      page = sysconf(_SC_PAGESIZE);
      status =
          page > 0 ? pthread_attr_setguardsize(&attributes, (size_t)page) : -1;
    }
    if (status == 0)
      status = pthread_setattr_default_np(&attributes);
    pthread_attr_destroy(&attributes);
  }
  if (status != 0) {
    fprintf(stderr, "cannot set Debug default worker stack to 128 KiB: %d\n",
            status);
    abort();
  }
}
