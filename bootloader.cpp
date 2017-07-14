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

#include "TwoWire.h"
#include "SelfProgram.h"

SelfProgram selfProgram;
volatile bool bootloaderRunning = true;

uint16_t getUInt16(uint8_t *data) {
	return data[0] | (data[1] << 8);
}

uint32_t getUInt32(uint8_t *data) {
	return data[0] | (data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

int checkDeviceID(uint8_t *data) {
	return (getUInt16(data) == selfProgram.getDeviceID());
}

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	return 0;
}


extern "C" {
	void runBootloader() {

		selfProgram.loadDeviceID();
		TwoWireInit(false /*useInterrupts*/);

		selfProgram.setLED(true);

		while (bootloaderRunning) {
			TwoWireUpdate();
		}
	}
};
