#include <SoftWire.h>
#include <ArduinoUnit.h>
#include <util/crc16.h>
#include "Constants.h"

#pragma once
// These are macros, so the caller's line number info and expressions
// are preserved
#define assertAck(result) assertEqual(result, SoftWire::ack)
#define assertOk(status) assertEqual(status, Status::COMMAND_OK)

inline uint8_t calcCrc(uint8_t first, uint8_t *rest, uint8_t len) {
        uint8_t crc = _crc8_ccitt_update(0, first);
        for (uint8_t i = 0; i < len; ++i)
          crc = _crc8_ccitt_update(crc, rest[i]);
        return crc;
}
