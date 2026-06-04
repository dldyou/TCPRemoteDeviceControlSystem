add_library(segment SHARED segment.c)
find_package(Threads REQUIRED)

target_include_directories(segment PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(segment PRIVATE wiringPi crypt Threads::Threads)
set_target_properties(segment PROPERTIES POSITION_INDEPENDENT_CODE ON)
