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
    auto payload = "C0 C0"; // payload w/ transport header + app header

    auto SUCCESS = fix.Parse(LPDU(payload));

    auto expectedEvents = {Event::LINK_FRAME, Event::TRANS_SEGMENT, Event::TRANS_PAYLOAD, Event::APP_REJECT };

    REQUIRE(fix.CheckEvents(expectedEvents));

    REQUIRE_FALSE(SUCCESS);
}