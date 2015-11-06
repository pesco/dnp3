cmake_minimum_required (VERSION 3.0)
project (proxy)
set(HAMMERDNP3_MAJOR_VERSION 1)
set(HAMMERDNP3_MINOR_VERSION 0)
set(HAMMERDNP3_MICRO_VERSION 0)
set(HAMMERDNP3_VERSION ${HAMMERDNP3_MAJOR_VERSION}.${HAMMERDNP3_MINOR_VERSION}.${HAMMERDNP3_MICRO_VERSION})

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)

# PkgConfig
FIND_PACKAGE(PkgConfig) # tell cmake to require pkg-config
PKG_CHECK_MODULES(GLIB2 REQUIRED glib-2.0>=2.36.0)

if(STATICLIBS)
      set(LIB_TYPE STATIC)
else()
      set(LIB_TYPE SHARED)
endif()

set(CMAKE_C_FLAGS "-Wall -std=c99")

# different release and debug flags
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
  
# include paths for all the local libraries
include_directories(include)
include_directories(src)

# for the tests
include_directories(${GLIB2_INCLUDE_DIRS})

# ---- library ----
file(GLOB_RECURSE dnp3hammer_SRC lib/src/dnp3hammer/*.c)
add_library(dnp3hammer ${LIB_TYPE} ${dnp3hammer_SRC})
target_link_libraries(dnp3hammer hammer)

# ---- test suite ----
add_executable(dnp3-tests ./test/unit/main.c)
target_link_libraries(dnp3-tests dnp3hammer ${GLIB2_LIBRARIES})


