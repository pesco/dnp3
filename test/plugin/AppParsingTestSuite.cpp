#include <catch.hpp>

#include "fixtures/PluginFixture.h"
#include "fixtures/DNP3Helpers.h"

#define SUITE(name) "Plugin - " name

TEST_CASE(SUITE("Construct and delete"))
{
    PluginFixture fix;
}

TEST_CASE(SUITE("rejects undersized ASDU"))
{
    PluginFixture fix;
    auto asdu = "C0 C0"; // payload w/ transport header + app header

    REQUIRE_FALSE(fix.Parse(LPDU(asdu)));
}