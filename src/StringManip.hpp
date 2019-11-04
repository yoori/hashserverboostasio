#ifndef GEARS_STRINGMANIP_HPP_
#define GEARS_STRINGMANIP_HPP_

// short copy of StringManip.hpp from gears without file dependencies ...
#include <exception>
#include <string>

namespace Gears
{
  namespace StringManip
  {
    /**
     * Encodes data into hex string in upcase letters alphabeth (A-F)
     * @param data source data
     * @param size data size
     * @param skip_leading_zeroes if skip all leading zeroes
     * @return encoded hex string
     */
    std::string
    hex_encode(
      const unsigned char* data,
      size_t size,
      bool skip_leading_zeroes)
      throw (std::exception);

    /**
     * Encodes data into hex string in lowcase letters alphabeth (a-f)
     * @param data source data
     * @param size data size
     * @param skip_leading_zeroes if skip all leading zeroes
     * @return encoded hex string
     */
    std::string
    hex_low_encode(
      const unsigned char* data,
      size_t size,
      bool skip_leading_zeroes)
      throw (std::exception);
  } // namespace StringManip
} // namespace Gears

// implementations
#endif /*GEARS_STRINGMANIP_HPP_*/
