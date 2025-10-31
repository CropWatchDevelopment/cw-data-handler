cmake_minimum_required(VERSION 3.10)
project(TTI_LavinMQ_Producer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.9.2
)
FetchContent_MakeAvailable(simdjson)

# Find mosquitto library for MQTT
find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

# Find librabbitmq for AMQP
pkg_check_modules(LIBRABBITMQ REQUIRED librabbitmq)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${MOSQUITTO_INCLUDE_DIRS} ${LIBRABBITMQ_INCLUDE_DIRS})

# Add executable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/mqtt_handler.cpp 
    src/lavinmq_publisher.cpp
    src/config_loader.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${LIBRABBITMQ_LIBRARIES} simdjson)
