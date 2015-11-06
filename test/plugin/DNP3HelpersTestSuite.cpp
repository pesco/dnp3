#include <catch.hpp>

#include "fixtures/DNP3Helpers.h"

#define SUITE(name) "DNP3Helpers - " name

/// these examples are from the DNP3 quick reference

TEST_CASE(SUITE("create link frames"))
{
    auto expected = "05 64 0E C4 01 00 00 04 7D A4 C0 C4 02 50 01 00 07 07 00 64 11";
    auto lpdu = LPDU("C0 C4 02 50 01 00 07 07 00");

    REQUIRE(lpdu == expected);
}

