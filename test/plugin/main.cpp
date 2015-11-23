
#define CATCH_CONFIG_RUNNER

#include <catch.hpp>

#include <dnp3hammer/dnp3.h>


int main( int argc, char* const argv[] )
{
  // global setup...
  dnp3_init();

  int result = Catch::Session().run( argc, argv );

  // global clean-up...

  return result;
}

