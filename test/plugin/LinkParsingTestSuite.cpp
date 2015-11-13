#include <catch.hpp>

#include "fixtures/PluginFixture.h"
#include "fixtures/DNP3Helpers.h"

#define SUITE(name) "PluginLink - " name

TEST_CASE(SUITE("Handles reset link states"))
{
    PluginFixture fix;

    REQUIRE(fix.Parse("05 64 05 C0 01 00 00 04 E9 21")); // reset link states
    fix.CheckEvents({Event::LINK_FRAME});
}

TEST_CASE(SUITE("Parses until encountering start octets"))
{
    PluginFixture fix;

    REQUIRE(fix.Parse("05 05 04 64 64 05 C0 01 00 00 04 E9 21")); // reset link states
    fix.CheckEvents({Event::LINK_FRAME});
}

