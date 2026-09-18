# Copyright (c) 2026 Real-Time Innovations, Inc.
# Subject to the Eclipse Public License v1.0; see LICENSE.md for details.

# Add Perftest Lite sources to an existing FreeRTOS/lwIP firmware target.
#
# Required variables:
#   PERFTEST_LITE_ROOT             Perftest Lite source directory
#   PERFTEST_LITE_RTIMEHOME        Connext DDS Micro installation directory
#   PERFTEST_LITE_RTIME_TARGET     Micro target library directory name
#   PERFTEST_LITE_RTIME_TARGET_PSL Micro PSL target library directory name
#   PERFTEST_LITE_RTI_OS_DEFINE    Micro OS definition, for example RTI_FREERTOS
#
# Optional variables:
#   PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES (default 1472)
#   PERFTEST_LITE_DISCOVERY                 (default DPDE; DPDE or DPSE)
#   PERFTEST_LITE_RTI_LIB_SUFFIX            (default z; use zd for debug)
#   PERFTEST_LITE_OS_SOURCE                 (default os_freertos_lwip.c)
#   PERFTEST_LITE_APP_SOURCE                (default app.c)

function(perftest_lite_add_embedded_role target role)
  foreach(required_variable IN ITEMS
      PERFTEST_LITE_ROOT
      PERFTEST_LITE_RTIMEHOME
      PERFTEST_LITE_RTIME_TARGET
      PERFTEST_LITE_RTIME_TARGET_PSL
      PERFTEST_LITE_RTI_OS_DEFINE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
      message(FATAL_ERROR "${required_variable} is required")
    endif()
  endforeach()

  string(TOUPPER "${role}" role_upper)
  if(NOT role_upper STREQUAL "PUB" AND NOT role_upper STREQUAL "SUB")
    message(FATAL_ERROR "Perftest Lite role must be PUB or SUB")
  endif()

  if(NOT DEFINED PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES)
    set(PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES 1472)
  endif()
  if(NOT DEFINED PERFTEST_LITE_DISCOVERY)
    set(PERFTEST_LITE_DISCOVERY DPDE)
  endif()
  string(TOUPPER "${PERFTEST_LITE_DISCOVERY}" discovery_upper)
  if(NOT discovery_upper STREQUAL "DPDE" AND NOT discovery_upper STREQUAL "DPSE")
    message(FATAL_ERROR "PERFTEST_LITE_DISCOVERY must be DPDE or DPSE")
  endif()
  if(NOT DEFINED PERFTEST_LITE_RTI_LIB_SUFFIX)
    set(PERFTEST_LITE_RTI_LIB_SUFFIX z)
  endif()

  set(example_dir "${PERFTEST_LITE_ROOT}/examples/embedded/freertos_lwip")
  if(NOT DEFINED PERFTEST_LITE_OS_SOURCE)
    set(PERFTEST_LITE_OS_SOURCE "${example_dir}/os_freertos_lwip.c")
  endif()
  if(NOT DEFINED PERFTEST_LITE_APP_SOURCE)
    set(PERFTEST_LITE_APP_SOURCE "${example_dir}/app.c")
  endif()
  set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/perftest_lite_gen_${target}")
  set(generation_config "${generated_dir}/generation-config.txt")
  file(GENERATE OUTPUT "${generation_config}" CONTENT
    "sequence_max_payload=${PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES}\n")
  set(generated_sources
    "${generated_dir}/perftest.c"
    "${generated_dir}/perftestPlugin.c"
    "${generated_dir}/perftestSupport.c")
  set(generated_headers
    "${generated_dir}/perftest.h"
    "${generated_dir}/perftestPlugin.h"
    "${generated_dir}/perftestSupport.h")

  add_custom_command(
    OUTPUT ${generated_sources} ${generated_headers}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_dir}"
    COMMAND "${PERFTEST_LITE_RTIMEHOME}/rtiddsgen/scripts/rtiddsgen"
      -language C -micro -interpreted 0
      -D "PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD=${PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES}"
      -verbosity 1 -update typefiles
      -d "${generated_dir}"
      "${PERFTEST_LITE_ROOT}/idl/sequence/perftest.idl"
    DEPENDS
      "${PERFTEST_LITE_ROOT}/idl/sequence/perftest.idl"
      "${generation_config}"
    VERBATIM)

  set(perftest_lite_sources
    "${PERFTEST_LITE_ROOT}/src/core/perftest_lite_args.c"
    "${PERFTEST_LITE_ROOT}/src/core/perftest_lite_main.c"
    "${PERFTEST_LITE_ROOT}/src/middleware/middleware_micro.c"
    "${PERFTEST_LITE_ROOT}/src/render/render_dispatch.c"
    "${PERFTEST_LITE_ROOT}/src/render/render_table.c"
    "${PERFTEST_LITE_ROOT}/src/type/type_default.c"
    "${PERFTEST_LITE_OS_SOURCE}"
    "${PERFTEST_LITE_APP_SOURCE}"
    ${generated_sources})

  if(role_upper STREQUAL "PUB")
    list(APPEND perftest_lite_sources
      "${PERFTEST_LITE_ROOT}/src/core/perftest_lite_pub.c")
    set(role_definition PERFTEST_LITE_BUILD_PUB=1)
  else()
    list(APPEND perftest_lite_sources
      "${PERFTEST_LITE_ROOT}/src/core/perftest_lite_sub.c")
    set(role_definition PERFTEST_LITE_BUILD_SUB=1)
  endif()

  target_sources(${target} PRIVATE ${perftest_lite_sources})
  target_include_directories(${target} PRIVATE
    "${PERFTEST_LITE_RTIMEHOME}/include"
    "${PERFTEST_LITE_RTIMEHOME}/include/rti_me"
    "${PERFTEST_LITE_ROOT}/src/core"
    "${PERFTEST_LITE_ROOT}/src/middleware"
    "${PERFTEST_LITE_ROOT}/src/os"
    "${PERFTEST_LITE_ROOT}/src/render"
    "${PERFTEST_LITE_ROOT}/src/type"
    "${example_dir}"
    "${generated_dir}")
  target_compile_features(${target} PRIVATE c_std_99)
  target_compile_definitions(${target} PRIVATE
    ${PERFTEST_LITE_RTI_OS_DEFINE}
    PERFTEST_LITE_OS_CUSTOM=1
    PERFTEST_LITE_HAS_RENDER_TABLE=1
    PERFTEST_LITE_PLATFORM_CONFIG_HEADER=\"platform_config.h\"
    ${role_definition}
    RTI_${discovery_upper}=1)

  set(micro_lib_dir
    "${PERFTEST_LITE_RTIMEHOME}/lib/${PERFTEST_LITE_RTIME_TARGET}")
  set(psl_lib_dir
    "${PERFTEST_LITE_RTIMEHOME}/lib/${PERFTEST_LITE_RTIME_TARGET_PSL}")
  string(TOLOWER "${discovery_upper}" discovery_lower)
  set(micro_libraries
    "${micro_lib_dir}/librti_me_disc${discovery_lower}${PERFTEST_LITE_RTI_LIB_SUFFIX}.a"
    "${micro_lib_dir}/librti_me_whsm${PERFTEST_LITE_RTI_LIB_SUFFIX}.a"
    "${micro_lib_dir}/librti_me_rhsm${PERFTEST_LITE_RTI_LIB_SUFFIX}.a"
    "${psl_lib_dir}/librti_me_netiopsl${PERFTEST_LITE_RTI_LIB_SUFFIX}.a"
    "${psl_lib_dir}/librti_me_ospsl${PERFTEST_LITE_RTI_LIB_SUFFIX}.a"
    "${micro_lib_dir}/librti_me${PERFTEST_LITE_RTI_LIB_SUFFIX}.a")

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(${target} PRIVATE
      "-Wl,--start-group" ${micro_libraries} "-Wl,--end-group")
  else()
    target_link_libraries(${target} PRIVATE ${micro_libraries})
  endif()
endfunction()
