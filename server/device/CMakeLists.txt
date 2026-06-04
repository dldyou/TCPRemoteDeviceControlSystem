add_subdirectory(led)
add_subdirectory(buzzer)
add_subdirectory(light)
add_subdirectory(segment)

add_library(server_device STATIC
    device_manager.c
    device_worker.c
    plugin_loader.c
    plugin_registry.c
    device_result.c
)

find_package(Threads REQUIRED)

target_include_directories(server_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(server_device PUBLIC
    Threads::Threads
    ${CMAKE_DL_LIBS}
)
