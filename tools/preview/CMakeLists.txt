# Display preview on the host, using the firmware's drawing code:
#   cmake -S tools/preview -B build-preview
#   cmake --build build-preview --target preview-run
# then open build-preview/preview.html
cmake_minimum_required(VERSION 3.13)
project(piedalera_preview C)
set(CMAKE_C_STANDARD 11)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

add_executable(preview
    preview.c
    ${SRC}/gfx.c
    ${SRC}/keyboard.c
    ${SRC}/notes.c
    ${SRC}/ui.c
)
target_include_directories(preview PRIVATE ${SRC})
target_compile_definitions(preview PRIVATE
    PREVIEW_TEMPLATE="${CMAKE_CURRENT_SOURCE_DIR}/template.html")
if(NOT MSVC)
    target_compile_options(preview PRIVATE -Wall -Wextra)
endif()

# Builds and renders in one step
add_custom_target(preview-run
    COMMAND preview ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS preview
    COMMENT "Rendering display preview")
