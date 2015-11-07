
#define CATCH_CONFIG_RUNNER

#include <catch.hpp>

#include <dnp3hammer/dissect.h>

int main( int argc, char* const argv[] )
{
  // global setup...
  if(dnp3_dissect_init(NULL) < 0) {
	std::cerr << "plugin init failed" << std::endl;
        return 1;
  }

  int result = Catch::Session().run( argc, argv );

  // global clean-up...

  return result;
}

