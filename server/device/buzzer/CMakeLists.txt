add_library(buzzer SHARED buzzer.c wav_reader.c pitch_detector.c)
find_package(Threads REQUIRED)

target_include_directories(buzzer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(buzzer PRIVATE wiringPi crypt Threads::Threads m)

set_target_properties(buzzer PROPERTIES POSITION_INDEPENDENT_CODE ON)
