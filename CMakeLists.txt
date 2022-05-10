cmake_minimum_required (VERSION 2.6)
project (skizo)

##################
#    Use C++11.  #
##################

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -m32 -fno-strict-aliasing")

###############################
#  Platform-specific settings
###############################

if (WIN32)
   add_definitions(-DSKIZO_WIN)
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthreads")
endif()

if(UNIX)
    add_definitions(-DSKIZO_X)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

####################
#  Common settings
####################

IF(CMAKE_BUILD_TYPE MATCHES Debug)
	add_definitions(-DSKIZO_DEBUG_MODE)
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

# Enable single-threaded mode for reference counting.
add_definitions(-DSKIZO_SINGLE_THREADED)

######################
#   Subdirectories.  #
######################

add_subdirectory(src)

#######################
#  Target executable.
#######################

add_executable(skizo launcher.cpp)

target_link_libraries(skizo skizort)
