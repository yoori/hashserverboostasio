#include <algorithm>

#include "StringManip.hpp"

namespace Gears
{
namespace StringManip
{
  namespace
  {
    const char HEX_UP_DIGITS[17] = "0123456789ABCDEF";
    const char HEX_LOW_DIGITS[17] = "0123456789abcdef";

    std::string
    hex_encode_ex(
      const unsigned char* data,
      size_t size,
      bool skip_leading_zeroes,
      const char* hex_digits)
      throw(std::exception)
    {
      if(!size)
      {
        return std::string();
      }

      if(skip_leading_zeroes)
      {
        while(!*data)
        {
          data++;
          if(!--size)
          {
            return std::string(1, '0');
          }
        }
      }

      std::string result;
      result.reserve(size * 2);
      if(skip_leading_zeroes && !((*data) & 0xF0))
      {
        result.push_back(hex_digits[*data]);
        data++;
        size--;
      }

      for (; size--; data++)
      {
        char buf[2] =
          {
            hex_digits[(*data) >> 4],
            hex_digits[(*data) & 0xF]
          };
        result.append(buf, 2);
      }

      return result;
    }
  }

  std::string
  hex_encode(
    const unsigned char* data,
    size_t size,
    bool skip_leading_zeroes)
    throw(std::exception)
  {
    return hex_encode_ex(
      data,
      size,
      skip_leading_zeroes,
      HEX_UP_DIGITS);
  }

  std::string
  hex_low_encode(
    const unsigned char* data,
    size_t size,
    bool skip_leading_zeroes)
    throw(std::exception)
  {
    return hex_encode_ex(
      data,
      size,
      skip_leading_zeroes,
      HEX_LOW_DIGITS);
  }
}
}
