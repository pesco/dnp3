#include <catch.hpp>

#include "fixtures/PluginFixture.h"

#define SUITE(name) "Plugin - " name

TEST_CASE(SUITE("Construct and delete"))
{
    PluginFixture fix;
}

TEST_CASE(SUITE("rejects undersized ASDU"))
{

    PluginFixture fix;
}