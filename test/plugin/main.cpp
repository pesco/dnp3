
#define CATCH_CONFIG_RUNNER

#include <catch.hpp>

#include <dnp3hammer.h>


int main( int argc, char* const argv[] )
{
  // global setup...
  dnp3_init();

  int result = Catch::Session().run( argc, argv );

  // global clean-up...

  return result;
}

