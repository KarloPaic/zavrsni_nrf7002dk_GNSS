cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_blink_test)

target_sources(app PRIVATE 
    ../../src/gps.c
    test_gps_parsing.c
)

target_include_directories(app PRIVATE
  ../../src
)
#target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
