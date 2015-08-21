cmake_minimum_required (VERSION 2.8)
project (dnp3hammer)
set(DNP3HAMMER_MAJOR_VERSION 1)
set(DNP3HAMMER_MINOR_VERSION 0)
set(DNP3HAMMER_MICRO_VERSION 0)
set(DNP3HAMMER_VERSION ${DNP3HAMMER_MAJOR_VERSION}.${DNP3HAMMER_MINOR_VERSION}.${DNP3HAMMER_MICRO_VERSION})

FIND_PACKAGE(PkgConfig) # tell cmake to require pkg-config

PKG_CHECK_MODULES(GLIB2 REQUIRED glib-2.0>=2.36.0)

add_definitions("-std=c99")

# include paths
include_directories(./include)
include_directories(${GLIB2_INCLUDE_DIRS})

# ---- library ----
file(GLOB_RECURSE dnp3hammer_SRC ./src/*.c)
add_library(dnp3hammer STATIC ${dnp3hammer_SRC})
target_link_libraries(dnp3hammer hammer)

# ---- test suite ----
add_executable(testsuite test.c)
target_link_libraries(testsuite dnp3hammer ${GLIB2_LIBRARIES})

# ---- crc checker -----
add_executable(crc crc.c)
target_link_libraries(crc dnp3hammer)

# ---- dissector -----
add_executable(dissect dissect.c)
target_link_libraries(dissect dnp3hammer)