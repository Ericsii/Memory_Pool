cmake_minimum_required(VERSION 3.14)
project(Memory_Pool)

set(CMAKE_CXX_STANDARD 11)

aux_source_directory(. DIR_SRCS)

add_executable(Memory_Pool ${DIR_SRCS})