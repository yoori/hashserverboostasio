#define __STDC_LIMIT_MACROS
#include <cstdint>
#include <endian.h>

#include "Hash.hpp"


#if SIZE_MAX != 18446744073709551615UL
#error std::size_t type must hold exactly 2^64 values.
#endif

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error Only little-endian platforms are supported.
#endif


namespace Gears
{
  namespace HashHelper
  {
    const std::size_t Murmur64::MULTIPLIER_;
    const std::size_t Murmur64::R_;
  }
}
