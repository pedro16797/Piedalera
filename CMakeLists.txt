cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Target board (pico, pico_w, pico2, pico2_w)")
include(pico_sdk_import.cmake)

project(piedalera C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(piedalera
    src/main.c
)

target_include_directories(piedalera PRIVATE src)
target_compile_options(piedalera PRIVATE -Wall -Wextra)

target_link_libraries(piedalera
    pico_stdlib
    pico_multicore
    hardware_i2c
)

# UART0 carries MIDI and GP0/GP1 are key inputs, so stdio goes over USB only.
pico_enable_stdio_usb(piedalera 1)
pico_enable_stdio_uart(piedalera 0)

pico_add_extra_outputs(piedalera)
