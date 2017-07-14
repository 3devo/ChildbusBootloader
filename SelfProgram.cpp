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

#define SIGRD 5

#include "SelfProgram.h"
#include "bootloader.h"

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

// The attiny841 has a 512 byte EEPROM section.
// The deviceID is at the end of the EEPROM section (address 510)
#define DEVICE_ID_ADDRESS ((uint16_t*)(476))

SelfProgram::SelfProgram() : _deviceID(0), _safeMode(true) {
}

void SelfProgram::setSafeMode(bool safeMode) {
	_safeMode = safeMode;
}

uint16_t SelfProgram::getDeviceID() {
	return _deviceID;
}

void SelfProgram::loadDeviceID() {
	_deviceID = eeprom_read_word(DEVICE_ID_ADDRESS);
	// 0xFFFF is an invalid device ID. Default to deviceID=1
	if (_deviceID == 0xFFFF) {
		_deviceID = 1;
	}
}

void SelfProgram::storeDeviceID(uint16_t deviceID) {
	_deviceID = deviceID;
	eeprom_write_word(DEVICE_ID_ADDRESS, _deviceID);
}

uint32_t SelfProgram::getSignature() {
	return (boot_signature_byte_get(0) |
			(((uint32_t)boot_signature_byte_get(0)) << 8) |
			(((uint32_t)boot_signature_byte_get(0)) << 16));
}

void SelfProgram::readEEPROM(uint16_t address, uint8_t *data,  uint8_t eeLen) {
	eeprom_read_block(data, (const void *)address, eeLen);
}

void SelfProgram::writeEEPROM(uint16_t address, uint8_t *data,  uint8_t eeLen) {
	eeprom_update_block(data, (void*)address, eeLen);
}

void SelfProgram::setLED(bool on) {
	DDRA |= _BV(5);
	if (on) {
		PORTA |= _BV(5);
	} else {
		PORTA &= ~_BV(5);
	}
}

// Flash page size is 16 bytes
int SelfProgram::getPageSize() {
	return SPM_PAGESIZE;
}

void SelfProgram::erasePage(uint32_t address) {
	// Mask of bits inside the page
	address &= ~(SPM_ERASESIZE - 1);

	// Bail if the address is past the application section.
	if (_safeMode and address >= 1024*6) {
		return;
	}

	boot_page_erase_safe(address);

	// If we just erased page 0, restore the reset vector in case
	// the bootloader gets interrupted before writing the page.
	// writePage() will change bytes 0 and 1.
	// All the other bytes in the page must remain erased (0xFF)
	if (_safeMode and address == 0) {
		uint8_t data[SPM_PAGESIZE];
		for (int i=0; i < SPM_PAGESIZE; i++) {
			data[i] = 0xFF;
		}
		writePage(0, data, SPM_PAGESIZE);
	}

	boot_spm_busy_wait();
}

int SelfProgram::readPage(uint32_t address, uint8_t *data, uint8_t len) {
	// Can only read at a 16 byte page boundary
	address &= ~(SPM_PAGESIZE - 1);

	for (int i=0; i < len; i++) {
		data[i] = pgm_read_byte(address+i);
	}

	return SPM_PAGESIZE;
}

void SelfProgram::writePage(uint32_t address, uint8_t *data, uint8_t len) {
	// Can only write to a 16 byte page boundary
	if (address % SPM_PAGESIZE != 0 || len > SPM_PAGESIZE) {
		return;
	}

	// If we are writing page 0, change the reset vector
	// so that it jumps to the bootloader.
	if (_safeMode and address == 0) {
		// Copy the reset vector from the data into the boot
		// trampoline area
		uint16_t instruction = data[0] | (data[1] << 8);
		writeTrampoline(instruction);
		data[0] = 0xFF;
		data[1] = 0xCB;
	}

	// If the address is past the application section don't write anything
	if (_safeMode and address >= 1024*6) {
		return;
	}

	// If we are the beginning of a 4-page boundary, erase it
	if (address % SPM_ERASESIZE == 0) {
		// If this is the page containing the trampoline, it
		// will already be erased when writing the first page
		if (address / SPM_ERASESIZE != (uint32_t)&startApplication / SPM_ERASESIZE)
			boot_page_erase_safe(address);
	}

	for (int i=0; i < len; i += 2) {
		uint16_t w = data[i] | (data[i+1] << 8);
		boot_page_fill_safe(address+i, w);
	}
	boot_page_write_safe(address);
}

void SelfProgram::writeTrampoline(uint16_t instruction) {
	uint32_t address = (uint32_t)&startApplication;
	// Erase the page containing the trampoline. This might erase
	// other application code. This makes no attempt to preserve the
	// other code to keep since simple. This should work since we're
	// writing a new application anyway.
	boot_page_erase_safe(address);

	// Take the reset instruction from the page at offset 0 and 1
	// and overwrite the trampoline with it
	boot_page_fill_safe(address, instruction);
	boot_page_write_safe(address);
}
