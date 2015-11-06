cmake_minimum_required (VERSION 3.0)
project (hammerdnp3)
set(HAMMERDNP3_MAJOR_VERSION 1)
set(HAMMERDNP3_MINOR_VERSION 0)
set(HAMMERDNP3_MICRO_VERSION 0)
set(HAMMERDNP3_VERSION ${HAMMERDNP3_MAJOR_VERSION}.${HAMMERDNP3_MINOR_VERSION}.${HAMMERDNP3_MICRO_VERSION})

# other options off-by-default that you can enable
option(WERROR "Set all warnings to errors" OFF)

# PkgConfig
FIND_PACKAGE(PkgConfig) # tell cmake to require pkg-config
PKG_CHECK_MODULES(GLIB2 REQUIRED glib-2.0>=2.36.0)

set(LIB_TYPE STATIC)

set(CMAKE_C_FLAGS "-Wall -std=c99")
set(CMAKE_CXX_FLAGS "-Wall -std=c++11")

# different release and debug flags
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
  
# include paths for all the local libraries
include_directories(lib/include)
# TODO - only add these for the required test projects
include_directories(lib/src)
include_directories(test/catch)

# for the tests
include_directories(${GLIB2_INCLUDE_DIRS})

# ---- parser library ----
file(GLOB_RECURSE dnp3hammer_SRC lib/src/dnp3hammer/*.c)
add_library(dnp3hammer ${LIB_TYPE} ${dnp3hammer_SRC})
target_link_libraries(dnp3hammer hammer)

# ---- unit test suite ----
add_executable(dnp3-tests ./test/unit/main.c)
target_link_libraries(dnp3-tests dnp3hammer ${GLIB2_LIBRARIES})

# ---- plugin test suite ----
file(GLOB_RECURSE plugintests_SRC test/plugin/*.cpp)
add_executable(plugin-tests ${plugintests_SRC})
target_link_libraries(plugin-tests dnp3hammer)

# ---- afl app layer program ----
add_executable(afl-app ./test/afl_app_layer/main.c)
target_link_libraries(afl-app dnp3hammer)

# ---- crc example program -----
add_executable(crc ./examples/crc/main.c)
target_link_libraries(crc dnp3hammer)

# ---- pprint example program -----
add_executable(pprint ./examples/plugins/plugin_main.c ./examples/plugins/pprint.c)
target_link_libraries(pprint dnp3hammer)

# ---- filter example program -----
add_executable(filter ./examples/plugins/plugin_main.c ./examples/plugins/filter.c)
target_link_libraries(filter dnp3hammer)

