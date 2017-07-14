/*
 * Copyright (C) 2015-2017 Erin Tomson <erin@rgba.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <util/crc16.h>

#include "TwoWire.h"
#include "SelfProgram.h"

struct Status {
	static const uint8_t COMMAND_OK            = 0x00;
	static const uint8_t COMMAND_FAILED        = 0x01;
	static const uint8_t COMMAND_NOT_SUPPORTED = 0x02;
	static const uint8_t INVALID_TRANSFER      = 0x03;
	static const uint8_t INVALID_CRC           = 0x04;
	static const uint8_t INVALID_ARGUMENTS     = 0x05;
};

struct Commands {
	static const uint8_t GET_PROTOCOL_VERSION  = 0x00;
	static const uint8_t SET_I2C_ADDRESS       = 0x01;
	static const uint8_t POWER_UP_DISPLAY      = 0x02;
	static const uint8_t GET_HARDWARE_INFO     = 0x03;
	static const uint8_t START_APPLICATION     = 0x04;
	static const uint8_t WRITE_FLASH           = 0x05;
	static const uint8_t FINALIZE_FLASH        = 0x06;
};

SelfProgram selfProgram;
volatile bool bootloaderRunning = true;

uint16_t getUInt16(uint8_t *data) {
	return data[0] | (data[1] << 8);
}

uint32_t getUInt32(uint8_t *data) {
	return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint8_t calcCrc(uint8_t *data, uint8_t len) {
	uint8_t crc = 0;
	for (uint8_t i = 0; i < len; ++i)
		crc = _crc8_ccitt_update(crc, data[i]);
	return crc;
}

static int processCommand(uint8_t *data, uint8_t len, uint8_t maxLen) {
	switch (data[0]) {
		default:
			data[0] = Status::COMMAND_NOT_SUPPORTED;
			return 1;
	}
}

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	// Check that there is at least room for a status byte and a CRC
	if (maxLen < 2)
		return 0;

	if (len < 2) {
		data[0] = Status::INVALID_TRANSFER;
		len = 1;
	} else {
		uint8_t crc = calcCrc(data, len);
		if (crc != 0) {
			data[0] = Status::INVALID_CRC;
			len = 1;
		} else {
			// CRC checks out, process a command
			len = processCommand(data, len - 1, maxLen - 1);
		}
	}

	data[len] = calcCrc(data, len);
	++len;

	return len;
}


extern "C" {
	void runBootloader() {

		TwoWireInit(false /*useInterrupts*/);

		while (bootloaderRunning) {
			TwoWireUpdate();
		}
	}
};
