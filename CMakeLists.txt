cmake_minimum_required(VERSION 3.10)
project(commesp C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)

file(GLOB SOURCES "src/*.c")

add_executable(espcommd ${SOURCES})

target_link_libraries(espcommd PRIVATE
    ubus
    ubox
    serialport
    blobmsg_json
)

target_compile_options(espcommd PRIVATE
    $<$<CONFIG:DEBUG>: -Wall -fsanitize=address -g >
)

target_link_options(espcommd PRIVATE
    $<$<CONFIG:DEBUG>: -fsanitize=address >
)
