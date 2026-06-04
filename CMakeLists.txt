cmake_minimum_required(VERSION 3.11)

project(TCPRemoteDeviceControlSystem)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_CLIENT "Build client component" ON)
option(BUILD_SERVER "Build server component" ON)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/common COMMON_SRCS)

add_library(common STATIC 
    common/app_config.c
    common/command.c
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

if (BUILD_CLIENT)
    add_subdirectory(client)
endif()

if (BUILD_SERVER)
    add_subdirectory(server)
endif()
