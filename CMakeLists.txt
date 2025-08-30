cmake_minimum_required(VERSION 3.10)
project(OpenCV-WebRTC-Streamer)

set(CMAKE_CXX_STANDARD 17)

find_package(LibDataChannel REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# GStreamer
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.14 gstreamer-app-1.0>=1.14)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/WebRTCStreamer.cpp
)

add_subdirectory(include/cpp-httplib)
add_subdirectory(include/spdlog)
add_subdirectory(include/CmdParser)

link_directories(${GST_LIBRARY_DIRS})

add_executable(main ${SOURCES})

target_link_libraries(main 
    LibDataChannel::LibDataChannel
    Threads::Threads
    nlohmann_json::nlohmann_json  
    ${OpenCV_LIBS}   
    ${GST_LIBRARIES}          # gstreamer-1.0 + gstreamer-app-1.0
    ${GST_LDFLAGS_OTHER}      # ensures gobject-2.0, glib-2.0, etc are linked
    ${RTC_LIBRARIES}
    spdlog::spdlog
)


set(STATIC_DIR ${CMAKE_SOURCE_DIR}/static)
set(OUTPUT_STATIC_DIR ${CMAKE_BINARY_DIR}/static)

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${STATIC_DIR} ${OUTPUT_STATIC_DIR}
    COMMENT "Copying static files to build directory"
)

