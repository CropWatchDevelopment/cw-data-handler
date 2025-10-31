cmake_minimum_required(VERSION 3.20)

project(cw-data-transfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# -------------------------------------------------------------------------------------------------
# Dependencies
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

FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.1
)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
FetchContent_GetProperties(libpqxx)
if(NOT libpqxx_POPULATED)
    FetchContent_Populate(libpqxx)
    set(BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(_libpqxx_config "${libpqxx_SOURCE_DIR}/cmake/config.cmake")
    if(EXISTS "${_libpqxx_config}")
        file(READ "${_libpqxx_config}" _libpqxx_config_contents)
        if(NOT _libpqxx_config_contents MATCHES "_cw_dt_module_dirs")
            file(WRITE "${_libpqxx_config}"
"set(_cw_dt_module_dirs \"\")\n"
"if(EXISTS \"${CMAKE_ROOT}/Modules/CMakeDetermineCompileFeatures.cmake\")\n"
"    list(APPEND _cw_dt_module_dirs \"${CMAKE_ROOT}/Modules\")\n"
"endif()\n"
"if(NOT _cw_dt_module_dirs)\n"
"    file(GLOB _cw_dt_module_dirs\n"
"        \"${CMAKE_ROOT}/../cmake-*/Modules\"\n"
"        \"${CMAKE_ROOT}/../cmake/Modules\"\n"
"        \"/usr/share/cmake-*/Modules\"\n"
"        \"/usr/local/share/cmake-*/Modules\")\n"
"endif()\n"
"if(_cw_dt_module_dirs)\n"
"    list(APPEND CMAKE_MODULE_PATH \${_cw_dt_module_dirs})\n"
"    list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)\n"
"endif()\n"
"unset(_cw_dt_module_dirs)\n"
"${_libpqxx_config_contents}")
        endif()
    endif()
    add_subdirectory(${libpqxx_SOURCE_DIR} ${libpqxx_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
    rabbitmqc
    GIT_REPOSITORY https://github.com/alanxz/rabbitmq-c.git
    GIT_TAG v0.14.0
)
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rabbitmqc)

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

# -------------------------------------------------------------------------------------------------
# Project sources
# -------------------------------------------------------------------------------------------------

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

if(TARGET rabbitmq::rabbitmq)
    set(RABBITMQ_TARGET rabbitmq::rabbitmq)
elseif(TARGET rabbitmq)
    set(RABBITMQ_TARGET rabbitmq)
else()
    message(FATAL_ERROR "RabbitMQ target was not created by FetchContent")
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simdjson
        pqxx
    ${RABBITMQ_TARGET}
    paho-mqtt3cs
)

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
