cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(8disp C CXX ASM)

pico_sdk_init()

add_executable(8disp
    main.c
    is31fl3730_display.c
)

target_include_directories(8disp PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(8disp pico_stdlib hardware_i2c)

pico_enable_stdio_usb(8disp 1)
pico_enable_stdio_uart(8disp 0)

pico_add_extra_outputs(8disp)
