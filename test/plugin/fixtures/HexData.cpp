
#include "HexData.h"

#include <stdexcept>
#include <sstream>

using namespace std;

HexData::HexData(const std::string& hex) :
        m_input(RemoveSpaces(hex)),
        m_size(Validate(m_input)),
        m_buffer(new uint8_t[m_size])
{
    size_t size = m_input.size();
    for(size_t index = 0, pos = 0; pos < size; ++index, pos += 2)
    {
        uint32_t val;
        std::stringstream ss;
        ss << std::hex << m_input.substr(pos, 2);
        if((ss >> val).fail())
        {
            throw std::invalid_argument(hex);
        }
        m_buffer[index] = static_cast<uint8_t>(val);
    }
}

std::string HexData::Convert(const uint8_t* buff, size_t length, bool spaced)
{
    std::ostringstream oss;
    size_t last = length - 1;
    for (size_t i = 0; i < length; i++)
    {
        char c = buff[i];
        oss << ToHexChar((c & 0xf0) >> 4) << ToHexChar(c & 0xf);
        if (spaced && i != last)oss << " ";
    }
    return oss.str();
}

char HexData::ToHexChar(char c)
{
    return (c > 9) ? ('A' + (c - 10)) : ('0' + c);
}

const uint8_t* HexData::Buffer() const
{
    return m_buffer.get();
}

size_t HexData::Size() const
{
    return m_size;
}

std::string HexData::RemoveSpaces(const std::string& hex)
{
    std::string copy(hex);
    RemoveSpacesInPlace(copy);
    return copy;
}

void HexData::RemoveSpacesInPlace(std::string& s)
{
    size_t pos = s.find_first_of(' ');
    if(pos != string::npos)
    {
        s.replace(pos, 1, "");
        RemoveSpacesInPlace(s);
    }
}

size_t HexData::Validate(const std::string& hex)
{
    for(char i : hex)
    {
        if(!IsHexChar(i))
        {
            throw std::invalid_argument(hex);
        }
    }

    if(hex.size() % 2)
    {
        throw std::invalid_argument(hex);
    }

    return hex.size() / 2;
}

bool HexData::IsHexChar(char i)
{
    return IsDigit(i) || IsUpperHexAlpha(i) || IsLowerHexAlpha(i);
}

bool HexData::IsDigit(char i)
{
    return (i >= '0') && (i <= '9');
}

bool HexData::IsUpperHexAlpha(char i)
{
    return (i >= 'A') && (i <= 'F');
}

bool HexData::IsLowerHexAlpha(char i)
{
    return (i >= 'a') && (i <= 'f');
}