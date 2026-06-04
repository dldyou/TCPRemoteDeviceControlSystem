set(CMAKE_INSTALL_RPATH "$ORIGIN")

add_subdirectory(device)

add_executable(server main.c parsing.c server_log.c socket.c)
add_dependencies(server led buzzer light segment)

find_package(Threads REQUIRED)

target_link_libraries(server PRIVATE 
    common
    server_device
    Threads::Threads
    m
    ${CMAKE_DL_LIBS}
)

set(CMAKE_INSTALL_RPATH "$ORIGIN")
