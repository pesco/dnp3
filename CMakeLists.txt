cmake_minimum_required (VERSION 2.8)
project (dnp3hammer)
set(DNP3HAMMER_MAJOR_VERSION 1)
set(DNP3HAMMER_MINOR_VERSION 0)
set(DNP3HAMMER_MICRO_VERSION 0)
set(DNP3HAMMER_VERSION ${DNP3HAMMER_MAJOR_VERSION}.${DNP3HAMMER_MINOR_VERSION}.${DNP3HAMMER_MICRO_VERSION})


set(CMAKE_CC_FLAGS "${CMAKE_CC_FLAGS} -Wall -std=c99 `pkg-config --cflags glib-2.0`")
set(CMAKE_REQUIRED_FLAGS ${CMAKE_CC_FLAGS})

# include paths for all the local libraries

include_directories(./include)

# ---- library ----
file(GLOB_RECURSE dnp3hammer_SRC ./src/*.c)
add_library(dnp3hammer STATIC ${openpal_SRC})



