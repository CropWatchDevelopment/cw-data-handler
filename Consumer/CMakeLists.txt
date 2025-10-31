cmake_minimum_required(VERSION 3.20)

project(cw-data-transfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# ------------------------------------------------------------
# simdjson
# ------------------------------------------------------------
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.1
)
set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
set(SIMDJSON_ENABLE_DEPRECATED_API OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)

# ------------------------------------------------------------
# libpqxx (robust)
# ------------------------------------------------------------
# 1) try official CMake package
message(STATUS "---- pqxx detection: step 1 (find_package) ----")
find_package(pqxx QUIET)

# we'll fill this
set(HAVE_PQXX_TARGET FALSE)

if(TARGET pqxx)
    message(STATUS "pqxx: found CMake target 'pqxx'")
    set(HAVE_PQXX_TARGET TRUE)
endif()

# 2) if not found, try pkg-config
if(NOT HAVE_PQXX_TARGET)
    message(STATUS "---- pqxx detection: step 2 (pkg-config) ----")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PQXX QUIET libpqxx)
        if(PQXX_FOUND)
            message(STATUS "pqxx: found via pkg-config: ${PQXX_VERSION}")
            add_library(pqxx INTERFACE)
            target_include_directories(pqxx INTERFACE ${PQXX_INCLUDE_DIRS})
            target_link_libraries(pqxx INTERFACE ${PQXX_LIBRARIES})
            set(HAVE_PQXX_TARGET TRUE)
        else()
            message(STATUS "pqxx: pkg-config did not find libpqxx")
        endif()
    else()
        message(STATUS "pqxx: pkg-config not available")
    endif()
endif()

# 3) if still not found, brute-force the library
if(NOT HAVE_PQXX_TARGET)
    message(STATUS "---- pqxx detection: step 3 (find_library) ----")
    find_path(PQXX_INCLUDE_DIR pqxx/pqxx
        /usr/include
        /usr/local/include
    )
    find_library(PQXX_LIBRARY NAMES pqxx
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
    )
    if(PQXX_LIBRARY)
        message(STATUS "pqxx: found library at ${PQXX_LIBRARY}")
        add_library(pqxx UNKNOWN IMPORTED)
        set_target_properties(pqxx PROPERTIES
            IMPORTED_LOCATION "${PQXX_LIBRARY}"
        )
        if(PQXX_INCLUDE_DIR)
            target_include_directories(pqxx INTERFACE "${PQXX_INCLUDE_DIR}")
        endif()
        # Also link libpq if present
        find_library(PQ_LIBRARY NAMES pq)
        if(PQ_LIBRARY)
            target_link_libraries(pqxx INTERFACE "${PQ_LIBRARY}")
        endif()
        set(HAVE_PQXX_TARGET TRUE)
    endif()
endif()

# 4) give up
if(NOT HAVE_PQXX_TARGET)
    message(FATAL_ERROR
        "libpqxx not found.\n"
        "Install it first, e.g. on Debian/Ubuntu/Raspberry Pi:\n"
        "  sudo apt-get install -y libpqxx-dev\n"
        "CI note: we expect the GitHub runner to have libpqxx-dev installed.")
endif()

# ------------------------------------------------------------
# rabbitmq-c
# ------------------------------------------------------------
FetchContent_Declare(
    rabbitmqc
    GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
    GIT_TAG v0.14.0
)
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rabbitmqc)

# ------------------------------------------------------------
# paho mqtt c
# ------------------------------------------------------------
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
    set(_paho_c_cmake "${paho_mqtt_c_SOURCE_DIR}/CMakeLists.txt")
    if(EXISTS "${_paho_c_cmake}")
        file(READ "${_paho_c_cmake}" _paho_c_contents)
        string(REPLACE "CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)"
                       "cmake_minimum_required(VERSION 3.5)"
                       _paho_c_contents "${_paho_c_contents}")
        file(WRITE "${_paho_c_cmake}" "${_paho_c_contents}")
    endif()
    add_subdirectory(${paho_mqtt_c_SOURCE_DIR} ${paho_mqtt_c_BINARY_DIR} EXCLUDE_FROM_ALL)
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
# rabbitmq target detection
# ------------------------------------------------------------
if(TARGET rabbitmq::rabbitmq)
    set(RABBITMQ_TARGET rabbitmq::rabbitmq)
elseif(TARGET rabbitmq)
    set(RABBITMQ_TARGET rabbitmq)
else()
    message(FATAL_ERROR "RabbitMQ target was not created by FetchContent")
endif()

# ------------------------------------------------------------
# final link
# ------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simdjson
        pqxx
        ${RABBITMQ_TARGET}
        paho-mqtt3cs
)

# ------------------------------------------------------------
# rabbitmq headers
# ------------------------------------------------------------
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
