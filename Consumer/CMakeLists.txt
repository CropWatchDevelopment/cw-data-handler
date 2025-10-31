cmake_minimum_required(VERSION 3.20)

project(cw-data-transfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# -------------------------------------------------------------------------------------------------
# simdjson (always fetched)
# -------------------------------------------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.1
)
set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
set(SIMDJSON_ENABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)

# -------------------------------------------------------------------------------------------------
# libpqxx – prefer system, only fetch if NOT found
# -------------------------------------------------------------------------------------------------
# 1) try system (this is what works on GitHub Actions because we did: apt-get install libpqxx-dev)
find_package(pqxx 7.8 QUIET)

if (NOT pqxx_FOUND)
    message(STATUS "System libpqxx not found – falling back to FetchContent")

    FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.9.1
    )

    # we do NOT touch libpqxx's cmake/config.cmake anymore
    set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(BUILD_DOC  OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(libpqxx)
else()
    message(STATUS "Using system libpqxx: ${pqxx_VERSION}")
endif()

# -------------------------------------------------------------------------------------------------
# rabbitmq-c
# -------------------------------------------------------------------------------------------------
FetchContent_Declare(
    rabbitmqc
    GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
    GIT_TAG v0.14.0
)
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rabbitmqc)

# -------------------------------------------------------------------------------------------------
# paho.mqtt.c
# -------------------------------------------------------------------------------------------------
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
FetchContent_MakeAvailable(paho_mqtt_c)

# -------------------------------------------------------------------------------------------------
# Project sources
# -------------------------------------------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

# pick rabbitmq target name
if(TARGET rabbitmq::rabbitmq)
    set(RABBITMQ_TARGET rabbitmq::rabbitmq)
elseif(TARGET rabbitmq)
    set(RABBITMQ_TARGET rabbitmq)
else()
    message(FATAL_ERROR "RabbitMQ target was not created by FetchContent")
endif()

# link
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simdjson
        ${RABBITMQ_TARGET}
        paho-mqtt3cs
)

# if we found system pqxx, link it
if (pqxx_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE pqxx)
else()
    # we fetched it – target is usually just "pqxx"
    target_link_libraries(${PROJECT_NAME} PRIVATE pqxx)
endif()

# include dirs for rabbitmq-c
set(RABBITMQ_INCLUDE_CANDIDATES
    "${rabbitmqc_SOURCE_DIR}/include"
    "${rabbitmqc_BINARY_DIR}/include"
    "${rabbitmqc_SOURCE_DIR}/librabbitmq"
    "${rabbitmqc_BINARY_DIR}/librabbitmq"
)

foreach(candidate ${RABBITMQ_INCLUDE_CANDIDATES})
    if(EXISTS "${candidate}/rabbitmq-c/amqp.h")
        target_include_directories(${PROJECT_NAME} PRIVATE "${candidate}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_RABBITMQ_C_AMQP_H)
        set(RABBITMQ_HEADER_FOUND TRUE)
        break()
    elseif(EXISTS "${candidate}/amqp.h")
        target_include_directories(${PROJECT_NAME} PRIVATE "${candidate}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_AMQP_H)
        set(RABBITMQ_HEADER_FOUND TRUE)
        break()
    endif()
endforeach()

if(NOT RABBITMQ_HEADER_FOUND)
    message(FATAL_ERROR "Failed to locate amqp.h in rabbitmq-c FetchContent tree")
endif()
