cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(led_blink_test)

target_sources(app PRIVATE 
    ../../src/led_blink.c
    test_led_blink.c
)

target_include_directories(app PRIVATE
  ../../src
)
