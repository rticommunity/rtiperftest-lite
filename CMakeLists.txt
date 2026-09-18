# Copyright (c) 2026 Real-Time Innovations, Inc.
# Subject to the Eclipse Public License v1.0; see LICENSE.md for details.

cmake_minimum_required(VERSION 3.15)
project(perftest_lite C)

# -------------------------------------------------------------------------
# Required inputs from the build script:
#   -DRTIMEHOME=<path to an RTI Connext DDS installation>
#   -DRTIME_TARGET_NAME=<platform>          (e.g. x86_64leElfgcc13.3.0)
#   -DRTIME_TARGET_PSL=<platform-PSL>       (e.g. x86_64leElfgcc13.3.0-Linux6)
# Optional:
#   -DPERFTEST_LITE_OS=linux                (default)
#   -DPERFTEST_LITE_MIDDLEWARE=micro        (default)
# -------------------------------------------------------------------------

if(NOT RTIMEHOME OR NOT RTIME_TARGET_NAME OR NOT RTIME_TARGET_PSL)
  message(FATAL_ERROR
    "RTIMEHOME, RTIME_TARGET_NAME, and RTIME_TARGET_PSL must be defined")
endif()

if(NOT PERFTEST_LITE_OS)
  set(PERFTEST_LITE_OS "linux")
endif()
if(NOT PERFTEST_LITE_MIDDLEWARE)
  set(PERFTEST_LITE_MIDDLEWARE "micro")
endif()

set(PERFTEST_LITE_TYPE "sequence" CACHE STRING "Type variant: sequence or zcopy")
set_property(CACHE PERFTEST_LITE_TYPE PROPERTY STRINGS sequence zcopy)
option(PERFTEST_LITE_ENABLE_ZEROCOPY "Enable Zero Copy v2 transport" OFF)
option(PERFTEST_LITE_ENABLE_SHMEM
  "Enable serialized Micro SHMEM transport when available" ON)
option(PERFTEST_LITE_INTERPRETED_TYPE
  "Use interpreted/XTypes generated type support" OFF)
set(PERFTEST_LITE_ZCOPY_DATALEN "64028" CACHE STRING
  "Fixed Zero Copy reported sample size in bytes, including overhead")
set(PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES "64000" CACHE STRING
  "Maximum payload accepted by the generated sequence type in bytes")
option(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS
  "Build OSAPI heap memory checkpoint instrumentation" OFF)
option(RTI_DPSE "Use static-endpoint discovery" OFF)
option(RTI_DPDE "Use dynamic-endpoint discovery" ON)

# Endpoint pools are selected at runtime from -latencyTest. Leave a cache
# value empty to use the default in perftest_lite_config.h; set an individual
# value to override just that limit without defining a whole build profile.
set(PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY "" CACHE STRING
  "Latency writer KEEP_LAST depth override")
set(PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY "" CACHE STRING
  "Latency writer maximum samples override")
set(PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY "" CACHE STRING
  "Latency reader KEEP_LAST depth override")
set(PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY "" CACHE STRING
  "Latency reader maximum samples override")
set(PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT "" CACHE STRING
  "Throughput writer KEEP_LAST depth override")
set(PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT "" CACHE STRING
  "Throughput writer maximum samples override")
set(PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT "" CACHE STRING
  "Throughput reader KEEP_LAST depth override")
set(PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT "" CACHE STRING
  "Throughput reader maximum samples override")
set(PERFTEST_LITE_RESOURCE_LIMIT_DEFINES)
foreach(PERFTEST_LITE_RESOURCE_LIMIT IN ITEMS
  PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY
  PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY
  PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY
  PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY
  PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT
  PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT
  PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT
  PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT)
  if(NOT "${${PERFTEST_LITE_RESOURCE_LIMIT}}" STREQUAL "")
  list(APPEND PERFTEST_LITE_RESOURCE_LIMIT_DEFINES
    ${PERFTEST_LITE_RESOURCE_LIMIT}=${${PERFTEST_LITE_RESOURCE_LIMIT}})
  endif()
endforeach()

if(PERFTEST_LITE_TYPE STREQUAL "zcopy")
  if(NOT PERFTEST_LITE_ZCOPY_DATALEN MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "PERFTEST_LITE_ZCOPY_DATALEN must be a positive integer")
  endif()
  if(PERFTEST_LITE_ZCOPY_DATALEN LESS_EQUAL 28)
    message(FATAL_ERROR "PERFTEST_LITE_ZCOPY_DATALEN must be greater than the 28-byte type overhead")
  endif()
  set(IDL_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/idl/zcopy/perftest.idl)
  set(IDL_GENERATION_FLAGS
      -D PERFTEST_TYPE_CONFIGURED_DATALEN=${PERFTEST_LITE_ZCOPY_DATALEN})
  set(TYPE_SRC_FILE src/type/type_zcopy.c)
    add_definitions(-DPERFTEST_LITE_TYPE_ZCOPY)
elseif(PERFTEST_LITE_TYPE STREQUAL "sequence")
  if(NOT PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES must be a positive integer")
  endif()
  set(IDL_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/idl/sequence/perftest.idl)
  set(IDL_GENERATION_FLAGS
      -D PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD=${PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES})
  set(TYPE_SRC_FILE src/type/type_default.c)
else()
  message(FATAL_ERROR "PERFTEST_LITE_TYPE must be 'sequence' or 'zcopy'")
endif()

if(PERFTEST_LITE_TYPE STREQUAL "zcopy" AND NOT PERFTEST_LITE_ENABLE_ZEROCOPY)
  message(WARNING "PERFTEST_LITE_TYPE=zcopy builds the fixed-array type without the Zero Copy transport; -transport ZeroCopy will be unavailable")
endif()

if(NOT PERFTEST_LITE_INTERPRETED_TYPE)
  list(APPEND IDL_GENERATION_FLAGS -interpreted 0)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
  ${RTIMEHOME}/include
  ${RTIMEHOME}/include/rti_me
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/os
  ${CMAKE_CURRENT_SOURCE_DIR}/src/type
  ${CMAKE_CURRENT_SOURCE_DIR}/src/middleware
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render
)

# -------------------------------------------------------------------------
# IDL generation
# -------------------------------------------------------------------------
set(GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN_DIR})

set(GEN_SRC
  ${GEN_DIR}/perftest.c
  ${GEN_DIR}/perftestPlugin.c
  ${GEN_DIR}/perftestSupport.c)

add_custom_command(
  OUTPUT ${GEN_SRC}
         ${GEN_DIR}/perftest.h
         ${GEN_DIR}/perftestPlugin.h
         ${GEN_DIR}/perftestSupport.h
  COMMAND ${RTIMEHOME}/rtiddsgen/scripts/rtiddsgen
      -language C -micro ${IDL_GENERATION_FLAGS} -verbosity 1 -update typefiles
      -d ${GEN_DIR} ${IDL_SOURCE_FILE}
    DEPENDS ${IDL_SOURCE_FILE}
    COMMENT "Generating ${PERFTEST_LITE_TYPE} Perftest type from IDL")

include_directories(${GEN_DIR})

# -------------------------------------------------------------------------
# Source layout (per the design's interface architecture)
# -------------------------------------------------------------------------
set(CORE_SRC
  src/core/perftest_lite_args.c
  src/core/perftest_lite_main.c
  src/render/render_dispatch.c
  ${TYPE_SRC_FILE})

set(PUB_SRC src/core/perftest_lite_pub.c)
set(SUB_SRC src/core/perftest_lite_sub.c)

option(PERFTEST_LITE_RENDER_TABLE "Build human-readable table renderer" ON)
option(PERFTEST_LITE_RENDER_CSV   "Build CSV renderer"                  OFF)
if(PERFTEST_LITE_RENDER_TABLE AND PERFTEST_LITE_RENDER_CSV)
  message(FATAL_ERROR "Enable exactly one of PERFTEST_LITE_RENDER_TABLE / _CSV")
endif()
if(PERFTEST_LITE_RENDER_TABLE)
  list(APPEND CORE_SRC src/render/render_table.c)
  add_definitions(-DPERFTEST_LITE_HAS_RENDER_TABLE)
elseif(PERFTEST_LITE_RENDER_CSV)
  list(APPEND CORE_SRC src/render/render_csv.c)
  add_definitions(-DPERFTEST_LITE_HAS_RENDER_CSV)
else()
  message(FATAL_ERROR "At least one renderer must be enabled")
endif()

if(PERFTEST_LITE_OS STREQUAL "linux")
  list(APPEND CORE_SRC src/os/os_linux.c)
  set(PERFTEST_LITE_OS_DEFINE PERFTEST_LITE_OS_LINUX)
  set(RTI_OS_DEFINE RTI_LINUX)
  # Micro's POSIX shared-memory transport (used by Zero Copy) resolves
  # shm_open() and shm_unlink() from librt on the lab's toolchain.
  set(PERFTEST_LITE_PLATFORM_LIBS pthread dl m rt)
  set(PERFTEST_LITE_PLATFORM_DEFINES "")
elseif(PERFTEST_LITE_OS STREQUAL "qnx")
  list(APPEND CORE_SRC src/os/os_qnx.c)
  set(PERFTEST_LITE_OS_DEFINE PERFTEST_LITE_OS_QNX)
  set(RTI_OS_DEFINE RTI_QNX)
  set(PERFTEST_LITE_PLATFORM_LIBS socket m)
  set(PERFTEST_LITE_PLATFORM_DEFINES
    PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE=8192
    PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE=41600
    PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE=8192)
else()
  message(FATAL_ERROR "PERFTEST_LITE_OS=${PERFTEST_LITE_OS} not implemented")
endif()

if(PERFTEST_LITE_MIDDLEWARE STREQUAL "micro")
  list(APPEND CORE_SRC src/middleware/middleware_micro.c)
  if(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    list(APPEND CORE_SRC src/middleware/memory_checkpoint.c)
    add_definitions(-DPERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
  endif()
else()
  message(FATAL_ERROR "PERFTEST_LITE_MIDDLEWARE=${PERFTEST_LITE_MIDDLEWARE} not implemented")
endif()

# -------------------------------------------------------------------------
# Library suffix (debug vs release)
# -------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(${CMAKE_BUILD_TYPE} MATCHES "[Rr]elease")
  set(RTI_LIB_SUFFIX "z")
  add_definitions(-DNDEBUG)
else()
  set(RTI_LIB_SUFFIX "zd")
endif()

# Use absolute paths to the static archives so we don't depend on
# `link_directories` propagation rules.
set(_LIBDIR_BASE ${RTIMEHOME}/lib/${RTIME_TARGET_NAME})
set(_LIBDIR_PSL  ${RTIMEHOME}/lib/${RTIME_TARGET_PSL})

# Regular SHMEM is a transport choice for the sequence type. It is distinct
# from Zero Copy v2, which also depends on the SHMEM type-plugin archive but
# uses a fixed-array SHMEM_REF type and the notification transport instead.
set(_SHMEM_LIB ${_LIBDIR_BASE}/librti_me_netioshmem${RTI_LIB_SUFFIX}.a)
set(_SHMEM_HDR ${RTIMEHOME}/include/rti_me/netio_shmem/netio_shmem.h)
set(PERFTEST_LITE_SHMEM_AVAILABLE OFF)
if(PERFTEST_LITE_ENABLE_SHMEM AND PERFTEST_LITE_TYPE STREQUAL "sequence")
  if(EXISTS "${_SHMEM_LIB}" AND EXISTS "${_SHMEM_HDR}")
    set(PERFTEST_LITE_SHMEM_AVAILABLE ON)
    add_definitions(-DPERFTEST_LITE_HAS_MICRO_SHMEM)
    message(STATUS "Serialized SHMEM transport: ENABLED (${_SHMEM_LIB})")
  else()
    message(STATUS "Serialized SHMEM transport: DISABLED (library or header not found)")
  endif()
elseif(PERFTEST_LITE_ENABLE_SHMEM)
  message(STATUS "Serialized SHMEM transport: DISABLED (requires PERFTEST_LITE_TYPE=sequence)")
endif()

if(PERFTEST_LITE_ENABLE_ZEROCOPY)
  if(NOT PERFTEST_LITE_TYPE STREQUAL "zcopy")
    message(FATAL_ERROR
      "PERFTEST_LITE_ENABLE_ZEROCOPY requires PERFTEST_LITE_TYPE=zcopy")
  endif()
  set(_ZC_LIB ${_LIBDIR_BASE}/librti_me_netiozcopy${RTI_LIB_SUFFIX}.a)
  set(_SDM_LIB ${_LIBDIR_BASE}/librti_me_netiosdm${RTI_LIB_SUFFIX}.a)
  set(_ZC_HDR ${RTIMEHOME}/include/rti_me/netio_zcopy/netio_zcopy.h)
  if(NOT EXISTS ${_ZC_LIB})
    message(FATAL_ERROR "Zero Copy library not found: ${_ZC_LIB}")
  endif()
  if(NOT EXISTS ${_SHMEM_LIB})
    message(FATAL_ERROR "Zero Copy SHMEM type-plugin library not found: ${_SHMEM_LIB}")
  endif()
  if(NOT EXISTS ${_SDM_LIB})
    message(FATAL_ERROR "Zero Copy SHMEM data-model library not found: ${_SDM_LIB}")
  endif()
  if(NOT EXISTS ${_ZC_HDR})
    message(FATAL_ERROR "Zero Copy header not found: ${_ZC_HDR}")
  endif()
  # rtiddsgen emits the SHMEM_REF plugin behind this Micro feature macro.
  # Without it, the fixed-array source compiles but registers the regular
  # type plugin, so DDS_DataWriter_get_loan() returns PRECONDITION_NOT_MET.
  add_definitions(-DPERFTEST_LITE_HAS_MICRO_ZEROCOPY -DRTIME_HAVE_ZCOPYV2=1)
endif()

# Select the discovery implementation and its matching static archive together.
# Explicitly enabling DPSE takes precedence over the default DPDE selection.
if(RTI_DPDE AND NOT RTI_DPSE)
  set(_DISCOVERY_PROFILE DPDE)
  set(_DISCOVERY_LIB ${_LIBDIR_BASE}/librti_me_discdpde${RTI_LIB_SUFFIX}.a)
  add_definitions(-DRTI_DPDE)
else()
  set(_DISCOVERY_PROFILE DPSE)
  set(_DISCOVERY_LIB ${_LIBDIR_BASE}/librti_me_discdpse${RTI_LIB_SUFFIX}.a)
  add_definitions(-DRTI_DPSE)
endif()
if(NOT EXISTS "${_DISCOVERY_LIB}")
  message(FATAL_ERROR
    "${_DISCOVERY_PROFILE} discovery library not found: ${_DISCOVERY_LIB}")
endif()
message(STATUS "Perftest Lite discovery: ${_DISCOVERY_PROFILE}")

set(MICRO_LIBS
  ${_DISCOVERY_LIB}
  ${_LIBDIR_BASE}/librti_me_whsm${RTI_LIB_SUFFIX}.a
  ${_LIBDIR_BASE}/librti_me_rhsm${RTI_LIB_SUFFIX}.a
  ${_LIBDIR_PSL}/librti_me_netiopsl${RTI_LIB_SUFFIX}.a
  ${_LIBDIR_PSL}/librti_me_ospsl${RTI_LIB_SUFFIX}.a
  ${_LIBDIR_BASE}/librti_me${RTI_LIB_SUFFIX}.a)

# Zero Copy v2's SHMEM data-model archive uses the Micro XCDR stream API even
# when the generated application type is non-interpreted. Keep the archive for
# that feature; regular and serialized-SHMEM builds do not need it.
if(PERFTEST_LITE_INTERPRETED_TYPE
  OR PERFTEST_LITE_ENABLE_ZEROCOPY)
  list(INSERT MICRO_LIBS 5
    ${_LIBDIR_BASE}/librti_me_ddsxtypes${RTI_LIB_SUFFIX}.a)
endif()

if(PERFTEST_LITE_ENABLE_ZEROCOPY)
  list(INSERT MICRO_LIBS 1 ${_ZC_LIB})
  list(INSERT MICRO_LIBS 1 ${_SHMEM_LIB})
  list(INSERT MICRO_LIBS 1 ${_SDM_LIB})
elseif(PERFTEST_LITE_SHMEM_AVAILABLE)
  list(INSERT MICRO_LIBS 1 ${_SHMEM_LIB})
endif()

# Two role-specific static libraries: each carries only the role's engine
# code (saves flash on embedded targets). Generated IDL sources are built
# once via an OBJECT library so the rtiddsgen custom command isn't raced
# by parallel builds of the two libs.
add_library(perftest_lite_gen OBJECT ${GEN_SRC})
target_compile_definitions(perftest_lite_gen PUBLIC ${RTI_OS_DEFINE} ${PERFTEST_LITE_OS_DEFINE})

add_library(perftest_lite_pub STATIC
  ${CORE_SRC} ${PUB_SRC} $<TARGET_OBJECTS:perftest_lite_gen>)
target_compile_definitions(perftest_lite_pub
  PUBLIC ${RTI_OS_DEFINE} ${PERFTEST_LITE_OS_DEFINE} ${PERFTEST_LITE_PLATFORM_DEFINES}
         ${PERFTEST_LITE_RESOURCE_LIMIT_DEFINES} PERFTEST_LITE_BUILD_PUB)

add_library(perftest_lite_sub STATIC
  ${CORE_SRC} ${SUB_SRC} $<TARGET_OBJECTS:perftest_lite_gen>)
target_compile_definitions(perftest_lite_sub
  PUBLIC ${RTI_OS_DEFINE} ${PERFTEST_LITE_OS_DEFINE} ${PERFTEST_LITE_PLATFORM_DEFINES}
         ${PERFTEST_LITE_RESOURCE_LIMIT_DEFINES} PERFTEST_LITE_BUILD_SUB)

add_executable(perftest_lite_publisher
  src/app/perftest_lite_app.c)

add_executable(perftest_lite_subscriber
  src/app/perftest_lite_app.c)

if(PERFTEST_LITE_TYPE STREQUAL "zcopy")
  set_target_properties(perftest_lite_publisher PROPERTIES OUTPUT_NAME "perftest_lite_publisher_zcopy")
  set_target_properties(perftest_lite_subscriber PROPERTIES OUTPUT_NAME "perftest_lite_subscriber_zcopy")
endif()

target_link_libraries(perftest_lite_publisher
  perftest_lite_pub
  -Wl,--start-group ${MICRO_LIBS} -Wl,--end-group
  ${PERFTEST_LITE_PLATFORM_LIBS})

target_link_libraries(perftest_lite_subscriber
  perftest_lite_sub
  -Wl,--start-group ${MICRO_LIBS} -Wl,--end-group
  ${PERFTEST_LITE_PLATFORM_LIBS})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(perftest_lite_pub        PRIVATE -Wall -Wextra -O3)
  target_compile_options(perftest_lite_sub        PRIVATE -Wall -Wextra -O3)
  target_compile_options(perftest_lite_publisher  PRIVATE -Wall -Wextra -O3)
  target_compile_options(perftest_lite_subscriber PRIVATE -Wall -Wextra -O3)
endif()
