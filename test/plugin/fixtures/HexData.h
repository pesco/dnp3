
#ifndef HEXDATA_H
#define HEXDATA_H

#include <string>
#include <cstdint>
#include <memory>

class HexData {

public:
    HexData(const std::string& hex);

    const uint8_t* Buffer() const;
    size_t Size() const;

    static std::string Convert(const uint8_t* pBuff, size_t length, bool spaced = false);

private:

    static char ToHexChar(char c);

    static std::string RemoveSpaces(const std::string& hex);

    static void RemoveSpacesInPlace(std::string& s);

    static size_t Validate(const std::string& hex);

    static bool IsHexChar(char i);
    static bool IsDigit(char i);
    static bool IsUpperHexAlpha(char i);
    static bool IsLowerHexAlpha(char i);

    std::string m_input;
    size_t m_size;
    std::unique_ptr<uint8_t[]> m_buffer;

};


#endif
