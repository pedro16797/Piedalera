# Host tests for the hardware-independent modules:
#   cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test
cmake_minimum_required(VERSION 3.13)
project(piedalera_tests C)
set(CMAKE_C_STANDARD 11)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(tests
    tests.c
    ${SRC}/app.c
    ${SRC}/config_mode.c
    ${SRC}/debounce.c
    ${SRC}/gfx.c
    ${SRC}/keyboard.c
    ${SRC}/notes.c
    ${SRC}/octave.c
    ${SRC}/settings.c
    ${SRC}/splash.c
    ${SRC}/ui.c
)
target_include_directories(tests PRIVATE ${SRC})
target_compile_options(tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(tests PRIVATE
    DEFAULT_INI="${CMAKE_CURRENT_SOURCE_DIR}/../config/piedalera.ini")

enable_testing()
add_test(NAME tests COMMAND tests)
