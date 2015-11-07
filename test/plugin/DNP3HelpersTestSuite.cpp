#include <catch.hpp>
#include <iostream>

#include "fixtures/DNP3Helpers.h"

#define SUITE(name) "DNP3Helpers - " name

/// these examples are from the DNP3 quick reference

TEST_CASE(SUITE("create link frames"))
{
    auto expected = "05 64 0E C4 01 00 00 04 7D A4 C0 C4 02 50 01 00 07 07 00 64 11";
    auto lpdu = LPDU("C0 C4 02 50 01 00 07 07 00");

    REQUIRE(lpdu == expected);
}

TEST_CASE(SUITE("create segments"))
{
    // segment and application layer frame into 4 transport segments with a sequence numbers starting at 62 that will wrap around
    auto frames = TPDUS("DEADBEEF", true, 1, 1024, 62, 1);
    auto expected = "05 64 07 C4 01 00 00 04 46 8B 7E DE AA 36 05 64 07 C4 01 00 00 04 46 8B 3F AD B7 A4 05 64 07 C4 01 00 00 04 46 8B 00 BE CA 88 05 64 07 C4 01 00 00 04 46 8B 81 EF C0 EF";

    REQUIRE(frames == expected);
}