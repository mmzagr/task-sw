cmake_minimum_required(VERSION 3.26)
project(task-sw C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS -pthread)

add_executable(task-sw task-sw.c task-sw.h task-sw-lib.c logger.c)
