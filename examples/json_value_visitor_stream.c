#include "lonejson.h"

#include <stdio.h>
#include <string.h>

/* Extract decoded string values from stdin, draining each chunk separately. */
typedef struct output_chunk {
  char bytes[1024];
  size_t length;
} output_chunk;

static lonejson_status accept_chunk(void *user, const char *data, size_t len,
                                    lonejson_error *error) {
  output_chunk *output = (output_chunk *)user;

  (void)error;
  if (len > sizeof(output->bytes)) {
    return LONEJSON_STATUS_OVERFLOW;
  }
  memcpy(output->bytes, data, len);
  output->length = len;
  return LONEJSON_STATUS_PAUSED;
}

static int drain_chunk(output_chunk *output) {
  /* With a nonblocking sink, wait for writability and retain the unwritten
   * suffix here. Resume the visitor only after this chunk has drained. */
  return fwrite(output->bytes, 1u, output->length, stdout) == output->length;
}

static int visit_input(lonejson_value_visitor_stream *stream,
                       output_chunk *output, lonejson_error *error) {
  unsigned char input[1024];
  size_t length;

  while ((length = fread(input, 1u, sizeof(input), stdin)) != 0u) {
    size_t offset = 0u;
    lonejson_status status;

    do {
      size_t consumed;

      status = stream->push(stream, input + offset, length - offset, &consumed,
                            error);
      offset += consumed;
      if (status == LONEJSON_STATUS_PAUSED && !drain_chunk(output)) {
        return 0;
      }
      if (status != LONEJSON_STATUS_OK && status != LONEJSON_STATUS_PAUSED) {
        return 0;
      }
    } while (status == LONEJSON_STATUS_PAUSED || offset < length);
  }
  return !ferror(stdin) &&
         stream->finish(stream, error) == LONEJSON_STATUS_OK &&
         fflush(stdout) == 0;
}

int main(void) {
  lonejson_error error;
  lonejson *runtime = lonejson_new(NULL, &error);
  lonejson_value_visitor visitor = lonejson_default_value_visitor();
  lonejson_value_visitor_stream *stream;
  output_chunk output;
  int ok;

  if (runtime == NULL) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  visitor.string_chunk = accept_chunk;
  stream =
      runtime->value_visitor_stream_open(runtime, &visitor, &output, &error);
  ok = stream != NULL && visit_input(stream, &output, &error);
  if (!ok) {
    fprintf(stderr, "%s\n", error.message[0] ? error.message : "I/O failed");
  }
  if (stream != NULL) {
    stream->close(stream);
  }
  lonejson_free(runtime);
  return ok ? 0 : 1;
}
