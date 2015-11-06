
#define CATCH_CONFIG_RUNNER

#include "../catch/catch.hpp"

#include <dnp3hammer/dissect.h>

// plugin hooks - do nothing for now
void hook_link_frame(DissectPlugin *self, const DNP3_Frame *frame, const uint8_t *buf, size_t len) {}
void hook_transport_reject(DissectPlugin *self) {}
void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment) {}
void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n) {}
void hook_app_reject(DissectPlugin *self) {}
void hook_app_error(DissectPlugin *self, DNP3_ParseError e) {}
void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment, const uint8_t *buf, size_t len) {}

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

