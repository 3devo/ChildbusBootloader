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

#ifndef SELFPROGRAM_H_
#define SELFPROGRAM_H_

#include <inttypes.h>

class SelfProgram {
public:
	SelfProgram();

	void setSafeMode(bool safeMode);

	void loadDeviceID();

	uint16_t getDeviceID();

	void storeDeviceID(uint16_t deviceID);

	uint32_t getSignature();

	void readEEPROM(uint16_t address, uint8_t *data,  uint8_t eeLen);

	void writeEEPROM(uint16_t address, uint8_t *data,  uint8_t len);

	void setLED(bool on);

	int getPageSize();

	void erasePage(uint32_t address);

	int readPage(uint32_t address, uint8_t *data, uint8_t len);

	void writePage(uint32_t address, uint8_t *data, uint8_t len);

	void startApplication();

	void checkBootMode();

	void jumpToApplication();

private:
	uint16_t _deviceID;
	bool _safeMode;
};

#endif /* SELFPROGRAM_H_ */
