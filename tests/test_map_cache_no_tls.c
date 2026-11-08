#define LONEJSON_TEST_FORCE_NO_TLS 1
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "test_support.inc.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

int main(void) {
  static lonejson_field fields[1];
  static lonejson_map map;

  EXPECT(LONEJSON__HAS_TLS == 0);
  EXPECT(LONEJSON__MAP_FLAG_CACHE_ENABLED == 0);

  memset(&map, 0, sizeof(map));
  map.name = "test_map_cache_no_tls";
  map.struct_size = sizeof(test_large_fixed_string_doc);
  map.fields = fields;
  map.field_count = 1u;

  fields[0] = test_large_fixed_string_doc_fields[0];
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(!lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_analysis_cached(&map).adopt_mask == 0u);

  fields[0] = test_alloc_parse_doc_fields[0];
  map.struct_size = sizeof(test_alloc_parse_doc);
  EXPECT(lonejson__map_layout_is_valid(&map));
  EXPECT(lonejson__map_may_allocate(&map));
  EXPECT(lonejson__map_analysis_cached(&map).adopt_mask == 1u);

  fields[0].struct_offset = sizeof(test_alloc_parse_doc);
  EXPECT(!lonejson__map_layout_is_valid(&map));

  return g_failures == 0 ? 0 : 1;
}
