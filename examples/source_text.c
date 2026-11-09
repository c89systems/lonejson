#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lonejson.h"

typedef struct outbound_text_doc {
  char id[16];
  lj_source text;
} outbound_text_doc;

static const lj_field outbound_text_doc_fields[] = {
    LJ_FIELD_STRING_FIXED_REQ(outbound_text_doc, id, "id", LJ_OVERFLOW_FAIL),
    LJ_FIELD_STRING_SOURCE(outbound_text_doc, text, "text")};
LJ_MAP_DEFINE(outbound_text_doc_map, outbound_text_doc,
              outbound_text_doc_fields);

int main(void) {
  static const char payload[] =
      "Hello from disk.\nThis is streamed as JSON text.";
  lj *runtime;
  outbound_text_doc doc;
  lj_error error;
  lj_status status;
  char path[] = "/tmp/lonejson-source-text-XXXXXX";
  int fd;
  char *json;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &outbound_text_doc_map, &doc, NULL);
  strcpy(doc.id, "txt-1");

  fd = mkstemp(path);
  if (fd < 0) {
    perror("mkstemp");
    return 1;
  }
  if (write(fd, payload, sizeof(payload) - 1u) !=
      (ssize_t)(sizeof(payload) - 1u)) {
    perror("write");
    close(fd);
    unlink(path);
    return 1;
  }
  close(fd);

  status = lj_source_set_path(&doc.text, path, &error);
  if (status != LJ_STATUS_OK) {
    fprintf(stderr, "set_path failed: %s\n", error.message);
    unlink(path);
    lj_free(runtime);
    return 1;
  }

  json =
      lj_serialize_alloc(runtime, &outbound_text_doc_map, &doc, NULL, &error);
  if (json == NULL) {
    fprintf(stderr, "serialize failed: %s\n", error.message);
    lj_cleanup(&outbound_text_doc_map, &doc, NULL);
    unlink(path);
    lj_free(runtime);
    return 1;
  }

  printf("source path=%s\n", path);
  printf("json=%s\n", json);

  LONEJSON_FREE(json);
  lj_cleanup(&outbound_text_doc_map, &doc, NULL);
  unlink(path);
  lj_free(runtime);
  return 0;
}
