cmake_minimum_required(VERSION 3.20)
project(cw-data-transfer LANGUAGES CXX)

# ------------------------------------------------------------
# C++ setup
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# pkg-config (for libpq, libpqxx)
# ------------------------------------------------------------
find_package(PkgConfig REQUIRED)

# ------------------------------------------------------------
# 1) libpq (C client)
# ------------------------------------------------------------
pkg_check_modules(LIBPQ REQUIRED libpq)

add_library(libpq_iface INTERFACE)
target_include_directories(libpq_iface INTERFACE ${LIBPQ_INCLUDE_DIRS})
target_link_libraries(libpq_iface INTERFACE ${LIBPQ_LIBRARIES})

# ------------------------------------------------------------
# 2) libpqxx (C++ client)
# ------------------------------------------------------------
find_package(pqxx QUIET)

if(NOT pqxx_FOUND)
    pkg_check_modules(PQXX REQUIRED libpqxx)
    add_library(pqxx_iface INTERFACE)
    target_include_directories(pqxx_iface INTERFACE ${PQXX_INCLUDE_DIRS})
    target_link_libraries(pqxx_iface INTERFACE ${PQXX_LIBRARIES})
    set(PQXX_TARGET pqxx_iface)
else()
    # system package that exports pqxx::pqxx
    set(PQXX_TARGET pqxx::pqxx)
endif()

# ------------------------------------------------------------
# 3) FetchContent deps
# ------------------------------------------------------------
include(FetchContent)

# simdjson ---------------------------------------------------
# Reuse an existing checkout when possible so CMake's FetchContent downloader
# never needs to retry a partially-cloned directory.
set(_simdjson_prefetched_dir "${CMAKE_SOURCE_DIR}/build-ci/_deps/simdjson-src")
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_SIMDJSON AND EXISTS "${_simdjson_prefetched_dir}/CMakeLists.txt")
    message(STATUS "Using vendored simdjson at ${_simdjson_prefetched_dir}")
    set(FETCHCONTENT_SOURCE_DIR_SIMDJSON "${_simdjson_prefetched_dir}" CACHE PATH "" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED_SIMDJSON ON CACHE BOOL "" FORCE)
endif()

if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_SIMDJSON)
    set(_simdjson_src_dir "${CMAKE_BINARY_DIR}/_deps/simdjson-src")
    if(NOT EXISTS "${_simdjson_src_dir}/CMakeLists.txt")
        find_package(Git REQUIRED)
        set(_simdjson_repo "https://github.com/simdjson/simdjson.git")
        set(_simdjson_tag "v3.9.1")
        set(_simdjson_clone_ok OFF)
        foreach(_try RANGE 1 3)
            if(EXISTS "${_simdjson_src_dir}")
                file(REMOVE_RECURSE "${_simdjson_src_dir}")
            endif()
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch "${_simdjson_tag}" --recurse-submodules --config "advice.detachedHead=false" "${_simdjson_repo}" "${_simdjson_src_dir}"
                RESULT_VARIABLE _git_result
                OUTPUT_QUIET
                ERROR_VARIABLE _git_error
            )
            if(NOT _git_result)
                set(_simdjson_clone_ok ON)
                break()
            endif()
            string(STRIP "${_git_error}" _git_error_stripped)
            message(WARNING "simdjson clone attempt ${_try} failed: ${_git_error_stripped}")
        endforeach()
        if(NOT _simdjson_clone_ok)
            message(FATAL_ERROR "Failed to clone simdjson (${_simdjson_repo} @ ${_simdjson_tag}). "
                                "Ensure network access or pre-populate FETCHCONTENT_SOURCE_DIR_SIMDJSON.")
        endif()
    endif()
    set(FETCHCONTENT_SOURCE_DIR_SIMDJSON "${_simdjson_src_dir}" CACHE PATH "" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED_SIMDJSON ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.1
)
set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
set(SIMDJSON_ENABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)

# rabbitmq-c -------------------------------------------------
FetchContent_Declare(
    rabbitmqc
    GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
    GIT_TAG v0.14.0
)
# match the way CI is building it
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rabbitmqc)

# paho-mqtt-c ------------------------------------------------
FetchContent_Declare(
    paho_mqtt_c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG v1.3.13
)
set(PAHO_WITH_SSL ON CACHE BOOL "" FORCE)
set(PAHO_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(PAHO_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_GetProperties(paho_mqtt_c)
if(NOT paho_mqtt_c_POPULATED)
    FetchContent_Populate(paho_mqtt_c)

    set(_paho_root "${paho_mqtt_c_SOURCE_DIR}/CMakeLists.txt")
    if(EXISTS "${_paho_root}")
        file(READ "${_paho_root}" _paho_cmake_contents)
        string(REPLACE "CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)"
                       "CMAKE_MINIMUM_REQUIRED(VERSION 3.5)"
                       _paho_cmake_contents "${_paho_cmake_contents}")
        string(REPLACE "cmake_minimum_required(VERSION 2.8.12)"
                       "cmake_minimum_required(VERSION 3.5)"
                       _paho_cmake_contents "${_paho_cmake_contents}")
        string(REPLACE "CMAKE_MINIMUM_REQUIRED(VERSION 2.8)"
                       "CMAKE_MINIMUM_REQUIRED(VERSION 3.5)"
                       _paho_cmake_contents "${_paho_cmake_contents}")
        string(REPLACE "cmake_minimum_required(VERSION 2.8)"
                       "cmake_minimum_required(VERSION 3.5)"
                       _paho_cmake_contents "${_paho_cmake_contents}")
        file(WRITE "${_paho_root}" "${_paho_cmake_contents}")
    endif()

    add_subdirectory("${paho_mqtt_c_SOURCE_DIR}" "${paho_mqtt_c_BINARY_DIR}")
endif()

# ------------------------------------------------------------
# sources
# ------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

# ------------------------------------------------------------
# rabbitmq target name normalization
# ------------------------------------------------------------
if(TARGET rabbitmq::rabbitmq)
    set(RABBITMQ_TARGET rabbitmq::rabbitmq)
elseif(TARGET rabbitmq)
    set(RABBITMQ_TARGET rabbitmq)
else()
    message(FATAL_ERROR "rabbitmq-c target not found (expected rabbitmq::rabbitmq or rabbitmq)")
endif()

# ------------------------------------------------------------
# rabbitmq-c *headers* for CI
#
# GitHub Actions builds rabbitmq-c via FetchContent, so the headers live under:
#   Consumer/build-ci/_deps/rabbitmqc-src/...
# NOT /usr/include.
#
# We just add every plausible dir instead of playing guessing games.
# ------------------------------------------------------------
# After FetchContent_MakeAvailable, these should exist:
#   rabbitmqc_SOURCE_DIR
#   rabbitmqc_BINARY_DIR
set(RABBITMQC_INCLUDE_DIRS
    "${rabbitmqc_SOURCE_DIR}"
    "${rabbitmqc_SOURCE_DIR}/include"
    "${rabbitmqc_SOURCE_DIR}/librabbitmq"
    "${rabbitmqc_BINARY_DIR}"
    "${rabbitmqc_BINARY_DIR}/include"
    "${rabbitmqc_BINARY_DIR}/librabbitmq"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${RABBITMQC_INCLUDE_DIRS})

# ------------------------------------------------------------
# link everything
# ------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simdjson
        ${PQXX_TARGET}     # libpqxx
        libpq_iface        # libpq
        ${RABBITMQ_TARGET} # rabbitmq-c
        paho-mqtt3cs       # from paho-mqtt-c
)
