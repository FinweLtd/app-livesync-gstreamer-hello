cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

# First, build socket.io cpp client
include(${CMAKE_CURRENT_SOURCE_DIR}/../../socket.io-client-cpp/CMakeLists.txt)

# Project name
project(livesync_gstreamer)

# Set C++ 14 standard
set(CMAKE_CXX_STANDARD 14)

# Find pkg-config (a helper tool)
find_package(PkgConfig)

# Use pkg-config for getting Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-sdp-1.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-webrtc-1.0)

# Use pkg-config for getting json-glib
pkg_check_modules(JSON-GLIB REQUIRED json-glib-1.0)

# Include GStreamer header files directory
include_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
        ${JSON-GLIB_INCLUDE_DIRS}
)

# Link GStreamer library directory
link_directories(
        ${GLIB_LIBRARY_DIRS}
        ${GSTREAMER_LIBRARY_DIRS}
)

# Build target executable
add_executable(${PROJECT_NAME} main.cpp)

# Link libraries with target executable
target_link_libraries(${PROJECT_NAME} sioclient_tls)
target_link_libraries(${PROJECT_NAME} gstsdp-1.0)
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} ${JSON-GLIB_LIBRARIES})

# Link Gstreamer library with target executable
target_link_libraries(
        ${PROJECT_NAME} 
        ${GSTREAMER_LIBRARIES} 
)
