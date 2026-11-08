#include "lonejson.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct record {
  lonejson_spooled text;
} record;
static const lonejson_field leaf_fields[] = {
    LONEJSON_FIELD_STRING_STREAM(record, text, "text")};
static const lonejson_field object_fields[] = {
    LONEJSON_FIELD_OBJECT(record, text, "child", NULL)};
LONEJSON_MAP_DEFINE(leaf_map, record, leaf_fields);

typedef struct allocation_state {
  size_t remaining;
  size_t live;
} allocation_state;
static void *allocate(void *user, size_t bytes) {
  allocation_state *state = (allocation_state *)user;
  void *result;
  if (state->remaining == 0u)
    return NULL;
  --state->remaining;
  result = malloc(bytes);
  if (result != NULL)
    ++state->live;
  return result;
}
static void *resize(void *user, void *ptr, size_t bytes) {
  allocation_state *state = (allocation_state *)user;
  if (ptr == NULL)
    return allocate(user, bytes);
  if (state->remaining == 0u)
    return NULL;
  --state->remaining;
  return realloc(ptr, bytes);
}
static void release(void *user, void *ptr) {
  allocation_state *state = (allocation_state *)user;
  if (ptr != NULL) {
    --state->live;
    free(ptr);
  }
}

typedef struct probe {
  lonejson_map *maps;
  lonejson_field *fields;
  size_t depth;
  const char *operation;
  int failed;
} probe;
#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);               \
      state->failed = 1;                                                       \
    }                                                                          \
  } while (0)

static void test_oom(probe *state, lonejson *runtime, allocation_state *alloc) {
  size_t budget;
  int operation;
  for (operation = 0; operation < 4; ++operation) {
    int failed = 0;
    int succeeded = 0;
    for (budget = 0u; budget < 16u; ++budget) {
      record value;
      record before;
      lonejson_error error;
      lonejson_status status;
      alloc->remaining = (size_t)-1;
      memset(&value, 0x5a, sizeof(value));
      if (operation == 1 || operation == 2) {
        CHECK(lonejson_init(runtime, &leaf_map, &value, &error) ==
              LONEJSON_STATUS_OK);
        CHECK(value.text.append(&value.text, "payload", 7u, &error) ==
              LONEJSON_STATUS_OK);
      }
      memcpy(&before, &value, sizeof(value));
      alloc->remaining = budget;
      error.truncated = operation == 3 ? 0 : 1;
      status =
          operation == 0 ? runtime->init(runtime, state->maps, &value, &error)
          : operation == 1
              ? runtime->reset(runtime, state->maps, &value, &error)
          : operation == 2
              ? runtime->cleanup(runtime, state->maps, &value, &error)
              : lonejson_parse_cstr(runtime, state->maps, &value, "{}", &error);
      alloc->remaining = (size_t)-1;
      if (operation != 3)
        CHECK(error.truncated == 0);
      if (status == LONEJSON_STATUS_ALLOCATION_FAILED) {
        ++failed;
        CHECK(error.code == status && error.message[0] != '\0');
        CHECK(memcmp(&before, &value, sizeof(value)) == 0);
        if (operation == 0 || operation == 3) {
          CHECK(lonejson_init(runtime, state->maps, &value, &error) ==
                LONEJSON_STATUS_OK);
        }
      } else {
        CHECK(status == LONEJSON_STATUS_OK);
        ++succeeded;
      }
      CHECK(lonejson_cleanup(state->maps, &value, &error) ==
            LONEJSON_STATUS_OK);
    }
    CHECK(failed != 0 && succeeded != 0);
  }
}

typedef struct node {
  lonejson_spooled text;
  lonejson_object_array children;
} node;
static const lonejson_field node_template[] = {
    LONEJSON_FIELD_STRING_STREAM(node, text, "text"),
    LONEJSON_FIELD_OBJECT_ARRAY(node, children, "children", node, NULL,
                                LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(node_template_map, node, node_template);

static void test_recursive_array(probe *state, lonejson *runtime,
                                 allocation_state *alloc) {
  lonejson_map map = node_template_map;
  lonejson_field fields[2];
  node *nodes = (node *)calloc(state->depth + 1u, sizeof(*nodes));
  lonejson_error error;
  size_t i;
  CHECK(nodes != NULL);
  if (nodes == NULL)
    return;
  memcpy(fields, node_template, sizeof(fields));
  fields[1].submap = &map;
  map.fields = fields;
  for (i = 0u; i <= state->depth; ++i) {
    CHECK(lonejson_init(runtime, &map, &nodes[i], &error) ==
          LONEJSON_STATUS_OK);
    CHECK(nodes[i].text.append(&nodes[i].text, "x", 1u, &error) ==
          LONEJSON_STATUS_OK);
    if (i != state->depth) {
      nodes[i].children.items = &nodes[i + 1u];
      nodes[i].children.count = nodes[i].children.capacity = 1u;
      nodes[i].children.elem_size = sizeof(*nodes);
      nodes[i].children.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
    }
  }
  {
    size_t bytes = (state->depth + 1u) * sizeof(*nodes);
    void *before = malloc(bytes);
    CHECK(before != NULL);
    if (before != NULL) {
      memcpy(before, nodes, bytes);
      alloc->remaining = 0u;
      CHECK(runtime->reset(runtime, &map, nodes, &error) ==
            LONEJSON_STATUS_ALLOCATION_FAILED);
      CHECK(memcmp(before, nodes, bytes) == 0);
      alloc->remaining = (size_t)-1;
      free(before);
    }
  }
  CHECK(runtime->reset(runtime, &map, nodes, &error) == LONEJSON_STATUS_OK);
  for (i = 0u; i <= state->depth; ++i) {
    CHECK(nodes[i].text.memory == NULL);
    CHECK(nodes[i].children.count == 0u);
  }
  CHECK(lonejson_cleanup(&map, nodes, &error) == LONEJSON_STATUS_OK);
  free(nodes);
}

static void *run_worker(void *user) {
  probe *state = (probe *)user;
  lonejson_error error;
  lonejson_config config = lonejson_default_config();
  lonejson_allocator allocator = lonejson_default_allocator();
  allocation_state alloc;
  lonejson *runtime;
  record value;
  int repeat;
  memset(&alloc, 0, sizeof(alloc));
  alloc.remaining = (size_t)-1;
  allocator.ctx = &alloc;
  allocator.malloc_fn = allocate;
  allocator.realloc_fn = resize;
  allocator.free_fn = release;
  config.allocator = &allocator;
  runtime = lonejson_new(&config, &error);
  CHECK(runtime != NULL);
  if (runtime == NULL)
    return NULL;
  if (strcmp(state->operation, "parse") == 0) {
    memset(&value, 0x5a, sizeof(value));
    CHECK(lonejson_parse_cstr(runtime, state->maps, &value, "{}", &error) ==
          LONEJSON_STATUS_OK);
    CHECK(value.text.append != NULL);
    CHECK(lonejson_cleanup(state->maps, &value, &error) == LONEJSON_STATUS_OK);
  } else if (strcmp(state->operation, "oom") == 0) {
    test_oom(state, runtime, &alloc);
  } else if (strcmp(state->operation, "arrays") == 0) {
    test_recursive_array(state, runtime, &alloc);
  } else if (strcmp(state->operation, "cycles") == 0) {
    record before;
    state->fields[0].submap = state->maps;
    memset(&value, 0x5a, sizeof(value));
    memcpy(&before, &value, sizeof(value));
    CHECK(lonejson_init(runtime, state->maps, &value, &error) ==
          LONEJSON_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&value, &before, sizeof(value)) == 0);
    CHECK(runtime->reset(runtime, state->maps, &value, &error) ==
          LONEJSON_STATUS_INVALID_ARGUMENT);
    CHECK(lonejson_cleanup(state->maps, &value, &error) ==
          LONEJSON_STATUS_INVALID_ARGUMENT);
    {
      int with_error;
      for (with_error = 0; with_error < 2; ++with_error) {
        lonejson_sse *sse = lonejson_sse_open(NULL, &error);
        CHECK(sse != NULL);
        if (sse != NULL) {
          CHECK(lonejson_sse_push_json(runtime, sse, state->maps, &value,
                                       "data: {}\n\n", 10u, NULL, NULL, NULL,
                                       with_error ? &error : NULL) ==
                LONEJSON_STATUS_INVALID_ARGUMENT);
          lonejson_sse_close(sse);
        }
      }
    }
  } else
    for (repeat = 0; repeat < 3; ++repeat) {
      const lonejson_map *initial =
          strcmp(state->operation, "init") == 0 ? state->maps : &leaf_map;
      memset(&value, 0x5a, sizeof(value));
      error.truncated = 1;
      CHECK(lonejson_init(runtime, initial, &value, &error) ==
            LONEJSON_STATUS_OK);
      CHECK(error.code == LONEJSON_STATUS_OK && error.truncated == 0);
      CHECK(value.text.append != NULL && value.text.cleanup != NULL);
      CHECK(value.text.append(&value.text, "payload", 7u, &error) ==
            LONEJSON_STATUS_OK);
      if (strcmp(state->operation, "reset") == 0) {
        CHECK(lonejson_reset(runtime, state->maps, &value, &error) ==
              LONEJSON_STATUS_OK);
        CHECK(value.text.memory == NULL && value.text.append != NULL);
      }
      CHECK(lonejson_cleanup(state->maps, &value, &error) ==
            LONEJSON_STATUS_OK);
      CHECK(lonejson_cleanup(state->maps, &value, &error) ==
            LONEJSON_STATUS_OK);
    }
  lonejson_free(runtime);
  CHECK(alloc.live == 0u);
  return NULL;
}

int main(int argc, char **argv) {
  probe state;
  pthread_t worker;
  size_t i;
  if (argc != 3)
    return 1;
  memset(&state, 0, sizeof(state));
  state.depth = (size_t)strtoul(argv[2], NULL, 10);
  state.operation = argv[1];
  state.maps = (lonejson_map *)calloc(state.depth + 1u, sizeof(*state.maps));
  state.fields =
      (lonejson_field *)calloc(state.depth + 1u, sizeof(*state.fields));
  if (state.maps == NULL || state.fields == NULL) {
    free(state.maps);
    free(state.fields);
    return 1;
  }
  for (i = 0u; i <= state.depth; ++i) {
    state.maps[i] = leaf_map;
    state.maps[i].fields = &state.fields[i];
    state.fields[i] = i == state.depth ? leaf_fields[0] : object_fields[0];
    if (i != state.depth)
      state.fields[i].submap = &state.maps[i + 1u];
  }
  fprintf(stderr, "default-stack worker: %s, map depth %lu\n", argv[1],
          (unsigned long)state.depth);
  /* No stack-size override: musl's normal worker stack is the regression. */
  if (pthread_create(&worker, NULL, run_worker, &state) != 0)
    state.failed = 1;
  else if (pthread_join(worker, NULL) != 0)
    return 1;
  free(state.maps);
  free(state.fields);
  return state.failed;
}
