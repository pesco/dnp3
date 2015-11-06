//
// Created by user on 11/6/15.
//

#ifndef HAMMERDNP3_DNP3HELPERS_H
#define HAMMERDNP3_DNP3HELPERS_H

#include <string>
#include <cstdint>

// creates 1 or more unconfirmed link-layer frames from an input ASDU
std::string AppToLink(const std::string& asdu, bool master = true, uint16_t dest = 1, uint16_t src = 1024, uint8_t segment = 249);

std::string LPDU(const std::string& payload, bool master = true, uint16_t dest = 1, uint16_t src = 1024);

void Write(uint8_t* dest, uint16_t value);

#endif //HAMMERDNP3_DNP3HELPERS_H
