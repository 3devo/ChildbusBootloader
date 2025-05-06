#pragma once

/*
Copyright 2017-2025 3devo (www.3devo.eu)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>

#ifdef __AVR__
// AVR has optimized assembly versions of some crc functions
#include <util/crc16.h>
#else
  // These are the equivalent C implementation as documented in the AVR
  // util/crc16.h header.

  inline uint8_t _crc8_ccitt_update (uint8_t inCrc, uint8_t inData)
  {
    uint8_t   i;
    uint8_t   data;

    data = inCrc ^ inData;

    for ( i = 0; i < 8; i++ )
    {
      if (( data & 0x80 ) != 0 )
      {
        data <<= 1;
        data ^= 0x07;
      }
      else
      {
        data <<= 1;
      }
    }
    return data;
  }

  inline uint16_t _crc_ccitt_update (uint16_t crc, uint8_t data)
  {
    data ^= (crc & 0xff);
    data ^= data << 4;

    return ((((uint16_t)data << 8) | (crc >> 8)) ^ (uint8_t)(data >> 4)
            ^ ((uint16_t)data << 3));
  }

  inline uint16_t _crc16_update(uint16_t crc, uint8_t a) {
    int i;

    crc ^= a;
    for (i = 0; i < 8; ++i) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = (crc >> 1);
    }

    return crc;
  }
#endif

/**
 * Helper class to calculate crcs for transfers. To use it, create an
 * instance, call update() for each byte and/or buffer of bytes to
 * calculate the CRC over, and then call get() to get the result.
 * update() is chainable, so you can do e.g.:
 *
 *   uint8_t crc = Crc<...>().update(first_byte).update(rest_of_bytes, len).get();
 */
template <typename T, T Update(T, uint8_t), T Initial>
class Crc {
  public:
    Crc& update(uint8_t b) {
      this->crc = Update(this->crc, b);
      return *this;
    }

    Crc& update(uint8_t *buf, uint8_t len) {
      for (uint8_t i = 0; i < len; ++i)
        this->update(buf[i]);
      return *this;
    }
    T get() {
      return this->crc;
    }

    Crc& reset() {
      this->crc = Initial;
      return *this;
    }

  private:
    T crc = Initial;
};

using Crc8Ccitt = Crc<uint8_t, _crc8_ccitt_update, 0xff>;
using Crc16Ccitt = Crc<uint16_t, _crc_ccitt_update, 0xffff>;
// Called CRC16-IBM (or CRC16-ANSI or just CRC16) by wikipedia, used by ModBus
using Crc16Ibm = Crc<uint16_t, _crc16_update, 0xffff>;
