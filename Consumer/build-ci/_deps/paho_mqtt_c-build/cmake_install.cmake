# Install script for directory: /home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/Eclipse Paho C/samples" TYPE FILE FILES
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTAsync_publish.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTAsync_publish_time.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTAsync_subscribe.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTClient_publish.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTClient_publish_async.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/MQTTClient_subscribe.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/paho_c_pub.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/paho_c_sub.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/paho_cs_pub.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/paho_cs_sub.c"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/src/samples/pubsub_opts.c"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/Eclipse Paho C" TYPE FILE FILES
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/CONTRIBUTING.md"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/epl-v20"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/edl-v10"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/README.md"
    "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-src/notice.html"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-build/src/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/kevin/source/C++/cw-data-transfer/Consumer/build-ci/_deps/paho_mqtt_c-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
