cmake_minimum_required (VERSION 2.6)
project (tcc)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")

add_definitions(-DTCC_TARGET_I386)
if (WIN32)
    add_definitions(-DTCC_TARGET_PE)
   file(GLOB TCC_SRC "*.c")
endif()
if (UNIX)
    file(GLOB TCC_SRC "i386-asm.c" "i386-gen.c" "libtcc.c" "tccasm.c" "tccelf.c" "tccgen.c" "tccpp.c" "tccrun.c")
endif()

add_library(tcc SHARED ${TCC_SRC})
