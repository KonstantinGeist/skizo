cmake_minimum_required (VERSION 3.0.2)

    #######################
    #  Pedantic!
    #######################

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wno-long-long -pedantic -Wno-variadic-macros -m32")

    ##################
    #  Source files. #
    ##################

file(GLOB SKIZO_SRC "*.cpp")
file(GLOB SKIZO_ICALLS_SRC "icalls/*.cpp")

#add_definitions(-DWITHOUT_LIBTCC)

    ##################
    #  Native stuff. #
    ##################

if (WIN32)
	file(GLOB SKIZO_NATIVE_SRC "native/win32/*.cpp")
endif()

if(UNIX)
    file(GLOB SKIZO_NATIVE_SRC "native/unix/*.cpp")
endif()

    #######################
    #  Target executable. #
    #######################
    
add_subdirectory(third-party/tcc)

add_library(skizort STATIC ${SKIZO_SRC} ${SKIZO_NATIVE_SRC} ${SKIZO_ICALLS_SRC})

    ################################
    #  Windows-specific libraries. #
    ################################

if(WIN32)
	target_link_libraries(skizort kernel32 user32 winmm Shlwapi psapi ws2_32 ole32 tcc)
endif()

    #############################
    #  Unix-specific libraries. #
    #############################

if(UNIX)
    target_link_libraries(skizort pthread dl tcc)
endif()
