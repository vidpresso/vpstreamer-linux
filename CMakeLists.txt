cmake_minimum_required(VERSION 3.7)
project(VPStreamer)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pointer-to-int-cast")

find_package(PNG REQUIRED)

set(OBS_INCLUDE_DIR $ENV{HOME}/code/git-vp/lobster/libobs-include)
message(STATUS "OBS_INCLUDE_DIR = ${OBS_INCLUDE_DIR}")

#add_library(obs SHARED IMPORTED)
#set_target_properties(obs PROPERTIES
#        IMPORTED_LOCATION "/usr/lib/libobs.so"
#        INTERFACE_INCLUDE_DIRECTORIES "${OBS_INCLUDE_DIR}"
#        )
#set(OBS_BASE_LIBRARIES obs X11 ${PNG_LIBRARIES} turbojpeg)

set(LOBSTER_LIBS
        rt m z dl
        avcodec avutil avformat swresample swscale
        x264
        mbedtls mbedcrypto mbedx509
        )

ADD_LIBRARY(lobster STATIC IMPORTED)
SET_TARGET_PROPERTIES(lobster PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lobster/liblobster.a"
        INTERFACE_LINK_LIBRARIES "${LOBSTER_LIBS}"
        )


set(SOURCE_FILES main.cpp image_utils.c vp_obs_video_shmem_source.cpp vp_obs_video_shmem_source.h vp_obs_audio_pipe_source.cpp vp_obs_audio_pipe_source.h sharedmem_consumer.c sharedmem_consumer.h vp_audio_pipe_consumer.cpp vp_audio_pipe_consumer.h time_utils.c time_utils.h vp_render_logger.cpp vp_render_logger.h)
add_executable(VPStreamer ${SOURCE_FILES})

add_definitions(-DUSE_LOBSTER)

target_include_directories(VPStreamer PUBLIC
        ${OBS_INCLUDE_DIR}
        )

target_link_libraries(VPStreamer
        -pthread
        ${PNG_LIBRARIES} turbojpeg
        #${OBS_BASE_LIBRARIES}
        #obs-opengl
        lobster
        )



