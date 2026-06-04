add_library(light SHARED light.c)
find_package(Threads REQUIRED)

target_include_directories(light PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(light PRIVATE wiringPi crypt Threads::Threads)
set_target_properties(light PROPERTIES POSITION_INDEPENDENT_CODE ON)
