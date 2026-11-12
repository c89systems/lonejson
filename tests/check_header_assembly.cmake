set(work "${ROOT}/build/header-assembly-test")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}/input")
set(public "#ifndef FIXTURE_H\n#define FIXTURE_H\n")
foreach(prefix LONEJSON LJ)
  foreach(component MAJOR MINOR PATCH)
    string(APPEND public "#define ${prefix}_VERSION_${component} 0\n")
  endforeach()
endforeach()
string(APPEND public "int fixture(void);\n#endif\n")
file(WRITE "${work}/input/header.h" "${public}")
file(WRITE "${work}/input/implementation.h" "int fixture(void) { return 42; }\n")
# A source archive must assemble headers without invoking a formatter. The
# sources, including whitespace in strings, must survive assembly unchanged.
execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DLONEJSON_ROOT=${ROOT}"
  "-DLONEJSON_PUBLIC_HEADER=${work}/input/header.h"
  "-DLONEJSON_INTERNAL_IMPL=${work}/input/implementation.h"
  "-DLONEJSON_SINGLE_HEADER_BUILD=${work}/out/header.h"
  "-DLONEJSON_SINGLE_HEADER_BUILD_ALIAS=${work}/alias/lonejson.h"
  "-DLONEJSON_SINGLE_HEADER_DIST_GZ=${work}/release.h.gz"
  "-DLONEJSON_CLANG_FORMAT_BIN=${work}/must-not-run"
  -DLONEJSON_VERSION=9.8.7
  -P "${ROOT}/cmake/package_single_header.cmake"
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Header assembly failed without a formatter")
endif()
file(READ "${work}/out/header.h" header)
file(READ "${work}/alias/lonejson.h" alias)
if(NOT header STREQUAL alias OR NOT header MATCHES "int fixture\\(void\\) \\{ return 42; \\}")
  message(FATAL_ERROR "Header aliases differ or assembly changed implementation bytes")
endif()
find_program(gzip NAMES gzip REQUIRED)
execute_process(COMMAND "${gzip}" -dc "${work}/release.h.gz"
  RESULT_VARIABLE status OUTPUT_VARIABLE release)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Cannot read assembled release header")
endif()
foreach(prefix LONEJSON LJ)
  foreach(pair "MAJOR;9" "MINOR;8" "PATCH;7")
    list(GET pair 0 component)
    list(GET pair 1 value)
    if(NOT release MATCHES "#define ${prefix}_VERSION_${component} ${value}([\r\n]|$)")
      message(FATAL_ERROR "Wrong release version: ${prefix}_VERSION_${component}")
    endif()
  endforeach()
endforeach()
file(REMOVE_RECURSE "${work}")
