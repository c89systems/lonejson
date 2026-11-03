include("${LONEJSON_ROOT}/cmake/lonejson_dist_dir.cmake")
lonejson_prepare_dist_dir()
set(dist_dir "${LONEJSON_DIST_DIR}")
set(source_root_name "lonejson-${LONEJSON_VERSION}")
set(archive_path "${dist_dir}/${source_root_name}.tar.gz")
set(stage_root "${LONEJSON_ROOT}/build/packaging/source")
set(archive_base "${stage_root}/${source_root_name}.tar")
set(stage_dir "${stage_root}/${source_root_name}")

include("${LONEJSON_ROOT}/cmake/lonejson_build_workspace.cmake")
lonejson_require_build_workspace("${stage_root}")

file(REMOVE_RECURSE "${stage_root}")
file(MAKE_DIRECTORY "${stage_root}")

execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/stage_release_sources.sh"
          "${LONEJSON_ROOT}" "${stage_dir}" "${LONEJSON_VERSION}"
  RESULT_VARIABLE stage_result
)
if(NOT stage_result EQUAL 0)
  file(REMOVE_RECURSE "${stage_root}")
  message(FATAL_ERROR "failed to stage source archive tree")
endif()

execute_process(
  COMMAND "${LONEJSON_ROOT}/scripts/archive_release_tree.sh"
    "${stage_dir}" "${archive_base}.gz"
  RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
  file(REMOVE_RECURSE "${stage_root}")
  message(FATAL_ERROR "failed to create source archive")
endif()

lonejson_publish_artifact("${archive_base}.gz" "${archive_path}")
file(REMOVE_RECURSE "${stage_root}")
