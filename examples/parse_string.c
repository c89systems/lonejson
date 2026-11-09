#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LONEJSON_IMPLEMENTATION
#include "lonejson.h"

typedef struct user_profile {
  char *name;
  lj_int64 age;
  bool active;
} user_profile;

static const lj_field user_profile_fields[] = {
    LJ_FIELD_STRING_ALLOC_REQ(user_profile, name, "name"),
    LJ_FIELD_I64(user_profile, age, "age"),
    LJ_FIELD_BOOL(user_profile, active, "active")};
LJ_MAP_DEFINE(user_profile_map, user_profile, user_profile_fields);

int main(void) {
  const char *json = "{\"name\":\"Alice\",\"age\":34,\"active\":true}";
  lj *runtime;
  user_profile profile;
  lj_error error;

  runtime = lj_new(NULL, &error);
  if (runtime == NULL) {
    fprintf(stderr, "runtime init failed: %s\n", error.message);
    return 1;
  }

  lj_init(runtime, &user_profile_map, &profile, NULL);
  if (lj_parse_cstr(runtime, &user_profile_map, &profile, json, &error) !=
      LJ_STATUS_OK) {
    fprintf(stderr, "parse failed: %s\n", error.message);
    lj_free(runtime);
    return 1;
  }

  printf("name=%s age=%ld active=%s\n", profile.name, (long)profile.age,
         profile.active ? "true" : "false");

  lj_cleanup(&user_profile_map, &profile, NULL);
  lj_free(runtime);
  return 0;
}
