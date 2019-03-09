cmake_minimum_required (VERSION 2.6)
project (tcc)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")

add_definitions(-DTCC_TARGET_I386)
add_definitions(-DTCC_TARGET_PE)

file(GLOB TCC_SRC "*.c")

add_library(tcc SHARED ${TCC_SRC})
