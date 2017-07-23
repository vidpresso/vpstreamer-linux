cmake_minimum_required(VERSION 3.7)
project(VPStreamer)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")


find_package(PNG REQUIRED)

set(OBS_INCLUDE_DIR $ENV{HOME}/code/obs-studio/libobs)
message(STATUS "OBS_INCLUDE_DIR = ${OBS_INCLUDE_DIR}")

add_library(obs SHARED IMPORTED)
set_target_properties(obs PROPERTIES
        IMPORTED_LOCATION "/usr/lib/libobs.so"
        INTERFACE_INCLUDE_DIRECTORIES "${OBS_INCLUDE_DIR}"
        )

set(OBS_BASE_LIBRARIES obs X11 ${PNG_LIBRARIES} turbojpeg)

set(SOURCE_FILES main.cpp image_utils.c vp_obs_video_shmem_source.c vp_obs_video_shmem_source.h vp_obs_audio_pipe_source.c vp_obs_audio_pipe_source.h sharedmem_consumer.c sharedmem_consumer.h)
add_executable(VPStreamer ${SOURCE_FILES})

target_include_directories(VPStreamer PUBLIC
        ${OBS_INCLUDE_DIR}
        )

target_link_libraries(VPStreamer -pthread rt ${OBS_BASE_LIBRARIES} obs-opengl)

