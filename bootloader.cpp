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
#include <avr/io.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <avr/wdt.h>

#include "Boards.h"
#include "TwoWire.h"
#include "SelfProgram.h"
#include "bootloader.h"

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

struct GeneralCallCommands {
	static const uint8_t RESET = 0x06;
	static const uint8_t RESET_ADDRESS = 0x04;
};

SelfProgram selfProgram;
volatile bool bootloaderRunning = true;

static uint16_t getUInt16(uint8_t *data) {
	return (uint16_t)data[0] << 8 | data[1];
}

static uint32_t getUInt32(uint8_t *data) {
	return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |  (data[2] << 8) | data[3];
}

static uint8_t calcCrc(uint8_t *data, uint8_t len) {
	uint8_t crc = 0;
	for (uint8_t i = 0; i < len; ++i)
		crc = _crc8_ccitt_update(crc, data[i]);
	return crc;
}

static uint8_t writeBuffer[SPM_ERASESIZE];
static uint16_t nextWriteAddress = 0;

static bool equalToFlash(uint16_t address, uint8_t len) {
	uint8_t offset = 0;
	while (len > 0) {
		if (writeBuffer[offset] != selfProgram.readByte(address + offset))
			return false;
		--len;
		++offset;
	}
	return true;
}


static void commitToFlash(uint16_t address, uint8_t len) {
	// If nothing needs to be changed, then don't
	if (equalToFlash(address, len))
		return;

	uint8_t offset = 0;
	while (len > 0) {
		uint8_t pageLen = len < SPM_PAGESIZE ? len : SPM_PAGESIZE;
		selfProgram.writePage(address + offset, &writeBuffer[offset], pageLen);
		len -= pageLen;
		offset += pageLen;
	}
}

static int handleWriteFlash(uint16_t address, uint8_t *data, uint8_t len) {
	if (address == 0)
		nextWriteAddress = 0;

	// Only consecutive writes are supported
	if (address != nextWriteAddress)
		return Status::INVALID_ARGUMENTS;

	nextWriteAddress += len;
	while (address < nextWriteAddress) {
		writeBuffer[address % SPM_ERASESIZE] = *data;
		++data;
		++address;

		if (address % SPM_ERASESIZE == 0)
			commitToFlash(address - SPM_ERASESIZE, SPM_ERASESIZE);
	}

	return Status::COMMAND_OK;
}

#ifdef HAVE_DISPLAY
void displayOn() {
	// This pin has a pullup to 3v3, so the display comes out of
	// reset as soon as the 3v3 is powered up. To prevent that, pull
	// it low now.
	*PIN_DISPLAY_RESET.port &= ~PIN_DISPLAY_RESET.mask;
	*PIN_DISPLAY_RESET.ddr |= PIN_DISPLAY_RESET.mask;

	// Reset sequence for the display according to datasheet: Enable
	// 3v3 logic supply, then release the reset, then powerup the
	// boost converter for LED power. This is a lot slower than
	// possible according to the datasheet.
	*PIN_3V3_ENABLE.ddr |= PIN_3V3_ENABLE.mask;
	*PIN_3V3_ENABLE.port |= PIN_3V3_ENABLE.mask;

        _delay_ms(1);
	// Switch to input to let external 3v3 pullup work instead of
	// making it high (which would be 5v);
	*PIN_DISPLAY_RESET.ddr &= ~PIN_DISPLAY_RESET.mask;

        _delay_ms(1);
	*PIN_BOOST_ENABLE.ddr |= PIN_BOOST_ENABLE.mask;
	*PIN_BOOST_ENABLE.port |= PIN_BOOST_ENABLE.mask;

        _delay_ms(5);
}
#endif

static int processCommand(uint8_t *data, uint8_t len, uint8_t maxLen) {
	if (maxLen < 5)
		return 0;

	switch (data[0]) {
		case Commands::GET_PROTOCOL_VERSION:
			data[0] = Status::COMMAND_OK;
			data[1] = 1;
			data[2] = 0;
			return 3;

		case Commands::SET_I2C_ADDRESS:
			// Only respond if the hw type in the request is
			// the wildcard or matches ours.
			if (data[2] != 0 && data[2] != INFO_HW_TYPE)
				return 0;

			TwoWireSetDeviceAddress(data[1]);
			data[0] = Status::COMMAND_OK;
			return 1;

		#ifdef HAVE_DISPLAY
		case Commands::POWER_UP_DISPLAY:
			displayOn();
			data[0] = Status::COMMAND_OK;
			data[1] = DISPLAY_CONTROLLER_TYPE;
			return 2;
		#endif

		case Commands::GET_HARDWARE_INFO:
			data[0] = Status::COMMAND_OK;
			data[1] = INFO_HW_TYPE;
			data[2] = INFO_HW_REVISION;
			data[3] = INFO_BL_VERSION;
			// Available flash size
			data[4] = (uint16_t)&startApplication;
			return 5;

		case Commands::START_APPLICATION:
			bootloaderRunning = false;
			// This is probably never sent
			data[0] = Status::COMMAND_OK;
			return 1;

		case Commands::WRITE_FLASH:
			data[0] = handleWriteFlash(getUInt16(data + 1), data + 3, len - 3);
			return 1;

		case Commands::FINALIZE_FLASH:
		{
			uint16_t pageAddress = nextWriteAddress & ~(SPM_PAGESIZE - 1);
			commitToFlash(pageAddress, nextWriteAddress - pageAddress);
			data[0] = Status::COMMAND_OK;
			return 1;
		}

		default:
			data[0] = Status::COMMAND_NOT_SUPPORTED;
			return 1;
	}
}

static int handleGeneralCall(uint8_t *data, uint8_t len, uint8_t /* maxLen */) {
	if (len >= 1 && data[0] == GeneralCallCommands::RESET) {
		wdt_enable(WDTO_15MS);
		while(true) /* wait */;

	} else if (len >= 1 && data[0] == GeneralCallCommands::RESET_ADDRESS) {
		TwoWireResetDeviceAddress();
	}
	return 0;
}

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	if (address == 0)
		return handleGeneralCall(data, len, maxLen);

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
}
