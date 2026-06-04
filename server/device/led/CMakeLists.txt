add_library(led SHARED led.c)
find_package(Threads REQUIRED)

target_include_directories(led PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(led PRIVATE wiringPi crypt Threads::Threads)
set_target_properties(led PROPERTIES POSITION_INDEPENDENT_CODE ON)
