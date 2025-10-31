cmake_minimum_required(VERSION 3.20)
project(cw-data-transfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# we may use pkg-config for libpq and libpqxx
find_package(PkgConfig REQUIRED)

# ------------------------------------------------------------
# 1) libpq (the C client, from libpq-dev)
# ------------------------------------------------------------
# On Ubuntu this works:
pkg_check_modules(LIBPQ REQUIRED libpq)

add_library(libpq_iface INTERFACE)
target_include_directories(libpq_iface INTERFACE ${LIBPQ_INCLUDE_DIRS})
target_link_libraries(libpq_iface INTERFACE ${LIBPQ_LIBRARIES})

# ------------------------------------------------------------
# 2) libpqxx (the C++ client, from libpqxx-dev)
# ------------------------------------------------------------
# First try official CMake package (some distros ship it). Ubuntu’s often doesn’t.
find_package(pqxx QUIET)

if(NOT pqxx_FOUND)
    # fall back to pkg-config
    pkg_check_modules(PQXX REQUIRED libpqxx)
    add_library(pqxx_iface INTERFACE)
    target_include_directories(pqxx_iface INTERFACE ${PQXX_INCLUDE_DIRS})
    target_link_libraries(pqxx_iface INTERFACE ${PQXX_LIBRARIES})
    set(PQXX_TARGET pqxx_iface)
else()
    # should give us pqxx::pqxx
    set(PQXX_TARGET pqxx::pqxx)
endif()

# ------------------------------------------------------------
# FetchContent for the stuff that actually needs to be fetched
# ------------------------------------------------------------
include(FetchContent)

# simdjson
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.1
)
set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
set(SIMDJSON_ENABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)

# rabbitmq-c
FetchContent_Declare(
    rabbitmqc
    GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
    GIT_TAG v0.14.0
)
# we'll let it build default, you can tune these:
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rabbitmqc)

# paho-mqtt-c
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

# ------------------------------------------------------------
# your sources
# ------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

# rabbitmq target name normalization
if(TARGET rabbitmq::rabbitmq)
    set(RABBITMQ_TARGET rabbitmq::rabbitmq)
elseif(TARGET rabbitmq)
    set(RABBITMQ_TARGET rabbitmq)
else()
    message(FATAL_ERROR "rabbitmq-c target not found")
endif()

# locate amqp headers (rabbitmq-c)
set(RABBITMQ_INCLUDE_CANDIDATES
    "${rabbitmqc_SOURCE_DIR}/include"
    "${rabbitmqc_BINARY_DIR}/include"
    "${rabbitmqc_SOURCE_DIR}/librabbitmq"
    "${rabbitmqc_BINARY_DIR}/librabbitmq"
)
set(RABBITMQ_HEADER_FOUND FALSE)
foreach(candidate ${RABBITMQ_INCLUDE_CANDIDATES})
    if(EXISTS "${candidate}/amqp.h")
        target_include_directories(${PROJECT_NAME} PRIVATE "${candidate}")
        set(RABBITMQ_HEADER_FOUND TRUE)
        break()
    elseif(EXISTS "${candidate}/rabbitmq-c/amqp.h")
        target_include_directories(${PROJECT_NAME} PRIVATE "${candidate}")
        set(RABBITMQ_HEADER_FOUND TRUE)
        break()
    endif()
endforeach()
if(NOT RABBITMQ_HEADER_FOUND)
    message(FATAL_ERROR "Could not find amqp.h from rabbitmq-c")
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simdjson
        ${PQXX_TARGET}         # libpqxx from system
        libpq_iface            # libpq from system
        ${RABBITMQ_TARGET}
        paho-mqtt3cs
)
