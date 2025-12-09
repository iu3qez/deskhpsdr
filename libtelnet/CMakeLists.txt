cmake_minimum_required(VERSION 3.10)

# libtelnet

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(telnet STATIC libtelnet.c)
