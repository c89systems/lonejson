include_guard(GLOBAL)

# Local executables are verification artifacts, never installed SDK content.
# On native Linux they run with the same complete Bootlin collection that
# built them. Keep this metadata private to executable targets: shipped
# libraries and exported package metadata must remain relocatable.
function(lonejson_configure_development_runtime target)
  set(_lonejson_extra_runtime_dirs "${LONEJSON_DEVELOPMENT_RUNTIME_EXTRA_DIRS}")
  get_target_property(_lonejson_target_type ${target} TYPE)
  if(NOT _lonejson_target_type STREQUAL "EXECUTABLE" OR
     NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
    return()
  endif()
  if(NOT LONEJSON_BOOTLIN_TOOLCHAIN_ROOT OR NOT CMAKE_SYSROOT)
    message(FATAL_ERROR
      "${target}: native Linux development executables require the selected Bootlin runtime")
  endif()

  if(LONEJSON_BOOTLIN_LIBC STREQUAL "musl")
    file(GLOB _lonejson_loader_candidates
      "${CMAKE_SYSROOT}/lib/ld-musl-*.so.1")
  else()
    file(GLOB _lonejson_loader_candidates
      "${CMAKE_SYSROOT}/lib/ld-linux*.so.2")
  endif()
  list(LENGTH _lonejson_loader_candidates _lonejson_loader_count)
  if(NOT _lonejson_loader_count EQUAL 1)
    message(FATAL_ERROR
      "${target}: expected one Bootlin dynamic loader under ${CMAKE_SYSROOT}/lib")
  endif()
  list(GET _lonejson_loader_candidates 0 _lonejson_loader)
  file(REAL_PATH "${_lonejson_loader}" _lonejson_loader)

  set(_lonejson_runtime_dirs
    "${CMAKE_SYSROOT}/lib"
    "${CMAKE_SYSROOT}/usr/lib"
    "${CMAKE_SYSROOT}/lib64"
    "${CMAKE_SYSROOT}/usr/lib64"
    # Sanitizer, C++, and compiler support DSOs live beside the target
    # collection rather than in its sysroot.  Keep these private to local
    # executables so ASan/UBSan/TSan never fall through to host runtimes.
    "${LONEJSON_BOOTLIN_TOOLCHAIN_ROOT}/${LONEJSON_BOOTLIN_TARGET_TRIPLE}/lib"
    "${LONEJSON_BOOTLIN_TOOLCHAIN_ROOT}/${LONEJSON_BOOTLIN_TARGET_TRIPLE}/lib64"
    "${CMAKE_CURRENT_BINARY_DIR}")
  if(LONEJSON_CPKT_ROOT)
    list(APPEND _lonejson_runtime_dirs "${LONEJSON_CPKT_ROOT}/lib")
  endif()
  if(_lonejson_extra_runtime_dirs)
    list(APPEND _lonejson_runtime_dirs
      ${_lonejson_extra_runtime_dirs})
  endif()
  set(_lonejson_existing_runtime_dirs)
  foreach(_lonejson_runtime_dir IN LISTS _lonejson_runtime_dirs)
    if(IS_DIRECTORY "${_lonejson_runtime_dir}")
      list(APPEND _lonejson_existing_runtime_dirs "${_lonejson_runtime_dir}")
    endif()
  endforeach()
  set(_lonejson_runtime_dirs ${_lonejson_existing_runtime_dirs})
  list(REMOVE_DUPLICATES _lonejson_runtime_dirs)
  list(JOIN _lonejson_runtime_dirs ":" _lonejson_runtime_rpath)

  set_property(TARGET ${target} PROPERTY SKIP_BUILD_RPATH TRUE)
  # GCC's sanitizer DSOs carry a $ORIGIN-only RUNPATH.  Make their libc,
  # libm, libgcc, and C++ dependencies direct executable dependencies so the
  # selected executable RPATH resolves them before a host default path.
  if(LONEJSON_ENABLE_ASAN OR LONEJSON_ENABLE_TSAN)
    target_link_options(${target} PRIVATE
      "LINKER:--no-as-needed,-lstdc++,-lm,-lgcc_s,--as-needed")
  endif()
  target_link_options(${target} PRIVATE
    "LINKER:--dynamic-linker,${_lonejson_loader}"
    "LINKER:--disable-new-dtags"
    "LINKER:-rpath,${_lonejson_runtime_rpath}")
  set_property(TARGET ${target} PROPERTY
    LONEJSON_DEVELOPMENT_RUNTIME_CONFIGURED TRUE)
endfunction()

function(lonejson_verify_development_runtime_coverage)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
    return()
  endif()

  get_property(_lonejson_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_lonejson_target IN LISTS _lonejson_targets)
    get_target_property(_lonejson_target_type ${_lonejson_target} TYPE)
    if(_lonejson_target_type STREQUAL "EXECUTABLE")
      get_target_property(_lonejson_runtime_configured
        ${_lonejson_target} LONEJSON_DEVELOPMENT_RUNTIME_CONFIGURED)
      if(NOT _lonejson_runtime_configured)
        message(FATAL_ERROR
          "${_lonejson_target}: native Linux executable is missing the Bootlin development runtime")
      endif()
    endif()
  endforeach()
endfunction()
