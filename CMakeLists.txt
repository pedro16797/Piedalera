cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Target board (pico, pico_w, pico2, pico2_w)")
include(pico_sdk_import.cmake)

project(piedalera C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(piedalera
    src/app.c
    src/config_mode.c
    src/debounce.c
    src/display.c
    src/gfx.c
    src/input.c
    src/keyboard.c
    src/main.c
    src/midi.c
    src/notes.c
    src/octave.c
    src/power.c
    src/settings.c
    src/splash.c
    src/storage.c
    src/ui.c
    src/widgets.c
)

target_include_directories(piedalera PRIVATE src)
target_compile_options(piedalera PRIVATE -Wall -Wextra -O2)

target_link_libraries(piedalera
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_flash
    hardware_i2c
    hardware_rosc
    hardware_sync
    hardware_xosc
)

# Run entirely from RAM: no flash cache misses, and flash writes on core 1
# can't stall core 0
pico_set_binary_type(piedalera copy_to_ram)

# UART0 carries MIDI and GP0/GP1 are keys; no stdio keeps core 0 free of
# USB interrupts
pico_enable_stdio_usb(piedalera 0)
pico_enable_stdio_uart(piedalera 0)

pico_add_extra_outputs(piedalera)
