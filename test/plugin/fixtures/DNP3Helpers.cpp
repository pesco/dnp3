#include "DNP3Helpers.h"

#include <catch.hpp>
#include "HexData.h"

#include <dnp3hammer/dnp3.h>
#include <cstring>

std::string AppToLink(const std::string& asdu, bool master, uint16_t dest, uint16_t src, uint8_t segment)
{
    return "";
}

std::string CreateLinkFrame(const std::string& data, bool master, uint16_t dest, uint16_t src)
{
    HexData payload(data);
    REQUIRE(payload.Size() <= 250);

    uint8_t lpdu[292];

    // write the header
    lpdu[0] = 0x05;
    lpdu[1] = 0x64;
    lpdu[2] = static_cast<uint8_t>(payload.Size() + 5);
    lpdu[3] = 0x44 | (master ? 0x80 : 0x00);
    Write(lpdu+4, dest);
    Write(lpdu+6, src);
    Write(lpdu+8, dnp3_crc(lpdu, 8)); // add the crc

    uint8_t* framePos = lpdu + 10;
    size_t payloadPos = 0;
    auto remaining = payload.Size();
    size_t frameSize = 10;

    while(remaining > 0)
    {
        const auto NUM = (remaining > 16) ? 16 : remaining;
        memcpy(framePos, payload.Buffer() + payloadPos, NUM);   //copy the data
        Write(framePos+NUM, dnp3_crc(framePos, NUM)); // write the CRC

        // update the loop variables
        framePos += NUM + 2;
        remaining -= NUM;
        payloadPos += NUM;
        frameSize += NUM + 2;
    }

    return HexData::Convert(lpdu, frameSize, true);
}

void Write(uint8_t* dest, uint16_t value)
{
    dest[0] = static_cast<uint8_t>(value & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}


