#include "DNP3Helpers.h"


#include "HexData.h"

#include <dnp3hammer.h>
#include <cstring>
#include <sstream>

using namespace std;

std::string TPDUS(const std::string& asdu, bool master, uint16_t dest, uint16_t src, uint8_t seqStart, uint8_t segmentSize)
{
    if (segmentSize < 1 || segmentSize > 249)
    {
        throw std::invalid_argument("bad segment size");
    }

    if(seqStart > 63)
    {
        throw std::invalid_argument("bad starting sequence number");
    }

    HexData data(asdu);

    ostringstream oss;

    auto pos = data.Buffer();
    auto remaining = data.Size();

    uint8_t seq  = seqStart;
    bool fir = true;

    while(remaining > 0)
    {
        const auto num = (remaining < segmentSize) ? remaining : segmentSize;

        const bool FIN = (remaining == num);
        uint8_t header = seq | (fir ? 0x40 : 0x0) | (FIN ? 0x80 : 0);

        ostringstream oss2;
        oss2 << HexData::Convert(&header, 1); // output the header
        oss2 << HexData::Convert(pos, num, false);

        if(!fir)
        {
            oss << " ";
        }
        oss << LPDU(oss2.str());

        // advance everything
        fir = false;
        seq = (seq+1) % 64;
        pos += num;
        remaining -= num;
    }

    return oss.str();
}

std::string LPDU(const std::string& data, bool master, uint16_t dest, uint16_t src)
{
    HexData payload(data);
    if(payload.Size() > 250) {
        throw std::invalid_argument("data must be <= 250 bytes in length");
    }

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


