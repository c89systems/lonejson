execute_process(COMMAND "${CMAKE_COMMAND}" -S "${ROOT}" -B "${WORK_DIR}"
    -G "${GENERATOR}" "-DCMAKE_TOOLCHAIN_FILE=${ROOT}/cmake/toolchains/native.cmake"
    -DCMAKE_BUILD_TYPE=Debug -DLONEJSON_BUILD_TESTS=OFF -DLONEJSON_BUILD_EXAMPLES=ON
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Debug application configure without tests failed: ${output}${diagnostic}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${WORK_DIR}"
    --target example_serialize_string RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Debug application build without tests failed: ${output}${diagnostic}")
endif()
execute_process(COMMAND "${WORK_DIR}/example_serialize_string"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 0 OR output STREQUAL "")
  message(FATAL_ERROR "Debug application startup failed: ${output}${diagnostic}")
endif()
