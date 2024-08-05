cmake_minimum_required(VERSION 3.28)
project(commesp C)

set(CMAKE_C_STANDARD 11)

add_executable(commesp
        main.c
        ubus.c
        ubus.h
        serial.h
        serial.c
        esp.h
        esp.c)

target_link_libraries(commesp ubus ubox serialport blobmsg_json)