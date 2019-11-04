#ifndef GEARS_CRC_HPP
#define GEARS_CRC_HPP

#include <cstddef>
#include <cstdint>

namespace Gears
{
  namespace CRC
  {
    /**
     * Calculates CRC32 of the supplied data
     * @param crc initial value of CRC
     * @param data data block
     * @param size its size
     */
    uint32_t
    quick(uint32_t crc, const void* data, size_t size) throw ();

    /**
     * Calculates reversed CRC32 of the supplied data
     * @param crc initial value of CRC
     * @param data data block
     * @param size its size
     */
    uint32_t
    reversed(uint32_t crc, const void* data, size_t size) throw ();
  }
}

namespace Gears
{
  namespace CRC
  {
    extern const uint32_t CRC_QUICK_TABLE[];

    inline
    uint32_t
    quick(uint32_t crc, const void* data, size_t size)
      throw ()
    {
      const uint8_t* udata = static_cast<const uint8_t*>(data);
      while (size-- > 0)
      {
        crc = (crc << 8) ^
          CRC_QUICK_TABLE[static_cast<uint8_t>(crc >> 24) ^ *udata++];
      }
      return crc;
    }

    extern const uint32_t CRC_REVERSED_TABLE[];

    inline
    uint32_t
    reversed(uint32_t crc, const void* data, size_t size)
      throw ()
    {
      const uint8_t* udata = static_cast<const uint8_t*>(data);
      crc = ~crc;
      while (size-- > 0)
      {
        crc = (crc >> 8) ^
          CRC_REVERSED_TABLE[static_cast<uint8_t>(crc) ^ *udata++];
      }
      return ~crc;
    }
  }
}

#endif /*GEARS_CRC_HPP*/
