#include "lonejson.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NODE_COUNT 28u
#define OUTPUT_CAPACITY (32u * 1024u)

typedef struct worker_node {
  lonejson_int64 value;
  lonejson_object_array children;
} worker_node;

static const lonejson_map worker_node_map;
static const lonejson_field worker_node_fields[] = {
    LONEJSON_FIELD_I64(worker_node, value, "value"),
    LONEJSON_FIELD_OBJECT_ARRAY(worker_node, children, "children", worker_node,
                                &worker_node_map, LONEJSON_OVERFLOW_FAIL)};
LONEJSON_MAP_DEFINE(worker_node_map, worker_node, worker_node_fields);

typedef struct worker_state {
  worker_node *nodes;
  char *output;
  char *expected;
  size_t length;
  int failed;
} worker_state;

static lonejson_status collect(void *user, const void *data, size_t length,
                               lonejson_error *error) {
  worker_state *state = (worker_state *)user;
  (void)error;
  if (length >= OUTPUT_CAPACITY - state->length) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(state->output + state->length, data, length);
  state->length += length;
  state->output[state->length] = '\0';
  return LONEJSON_STATUS_OK;
}

static void *run_worker(void *user) {
  worker_state *state = (worker_state *)user;
  lonejson_config config;
  lonejson_error error;
  lonejson *runtime;
  int pretty;
  size_t index;
  size_t compared;
  for (pretty = 0; pretty <= 1; ++pretty) {
    config = lonejson_default_config();
    config.write_pretty = pretty;
    runtime = lonejson_new(&config, &error);
    if (runtime == NULL) {
      state->failed = 1;
      return NULL;
    }
    state->length = 0u;
    if (lonejson_serialize_sink(runtime, &worker_node_map, state->nodes,
                                collect, state, &error) != LONEJSON_STATUS_OK ||
        state->length == 0u ||
        strstr(state->output, pretty ? "\"value\": 27" : "\"value\":27") ==
            NULL) {
      fprintf(stderr, "worker serialization failed: %s\n", error.message);
      state->failed = 1;
    }
    compared = 0u;
    for (index = 0u; index < state->length; ++index) {
      char byte = state->output[index];
      if (byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t') {
        continue;
      }
      if (byte != state->expected[compared++]) {
        state->failed = 1;
        break;
      }
    }
    if (compared != strlen(state->expected)) {
      state->failed = 1;
    }
    lonejson_free(runtime);
    if (state->failed) {
      return NULL;
    }
  }
  return NULL;
}

int main(void) {
  worker_state state;
  pthread_t thread;
  pthread_attr_t attributes;
  size_t index;
  size_t default_stack;
  char prefix[64];
  if (pthread_attr_init(&attributes) != 0) {
    return 1;
  }
  if (pthread_attr_getstacksize(&attributes, &default_stack) != 0) {
    pthread_attr_destroy(&attributes);
    return 1;
  }
  pthread_attr_destroy(&attributes);
  printf("default worker stack: %lu bytes\n", (unsigned long)default_stack);
  fflush(stdout);
  memset(&state, 0, sizeof(state));
  state.nodes = (worker_node *)calloc(NODE_COUNT, sizeof(*state.nodes));
  state.output = (char *)malloc(OUTPUT_CAPACITY);
  state.expected = (char *)calloc(OUTPUT_CAPACITY, 1u);
  if (state.nodes == NULL || state.output == NULL || state.expected == NULL) {
    free(state.nodes);
    free(state.output);
    free(state.expected);
    return 1;
  }
  for (index = 0u; index < NODE_COUNT; ++index) {
    state.nodes[index].value = (lonejson_int64)index;
    state.nodes[index].children.elem_size = sizeof(*state.nodes);
    state.nodes[index].children.flags = LONEJSON_ARRAY_FIXED_CAPACITY;
    sprintf(prefix, "{\"value\":%lu,\"children\":[", (unsigned long)index);
    strcat(state.expected, prefix);
    if (index + 1u < NODE_COUNT) {
      state.nodes[index].children.items = &state.nodes[index + 1u];
      state.nodes[index].children.count = 1u;
      state.nodes[index].children.capacity = 1u;
    }
  }
  for (index = 0u; index < NODE_COUNT; ++index) {
    strcat(state.expected, "]}");
  }
  /* NULL attributes deliberately retain the platform's default worker stack. */
  if (pthread_create(&thread, NULL, run_worker, &state) != 0) {
    state.failed = 1;
  } else if (pthread_join(thread, NULL) != 0) {
    return 1;
  }
  free(state.nodes);
  free(state.output);
  free(state.expected);
  return state.failed;
}
