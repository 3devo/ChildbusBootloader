/*
 * SelfProgram.cpp
 *
 * Created: 11/12/2015 9:55:15 AM
 *  Author: ekt
 */

#define SIGRD 5

#include "SelfProgram.h"

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

void SelfProgram::getSignature(uint8_t *data, int len) {
	data[0] = boot_signature_byte_get(0);
	data[1] = boot_signature_byte_get(2);
	data[2] = boot_signature_byte_get(4);			
}

void SelfProgram::readEEPROM(uint8_t *data, void *address, int eeLen) {
	eeprom_read_block(data, address, eeLen);
}

void SelfProgram::writeEEPROM(uint8_t *data, void *address, int len) {
	eeprom_update_block(data, (void*)address, len);
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

void SelfProgram::erasePage(uint16_t address) {
	// Can only write to a 64 byte 4 page boundary, so mask the lower 6 bits
	address &= 0xFFC0;
	
	// Bail if the address is past the application section.
	if (_safeMode and address >= 1024*6) {
		return;
	}
	
	//eeprom_busy_wait();
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
		writePage(0, data);
	}
	
	boot_spm_busy_wait();
}

int SelfProgram::readPage(uint8_t address, uint8_t *data) {
	// Can only read at a 16 byte page boundary
	address &= 0xFFF0;
	
	for (int i=0; i < SPM_PAGESIZE; i++) {
		data[i] = pgm_read_byte(address+i);
	}
	
	return SPM_PAGESIZE;
}

void SelfProgram::writePage(uint16_t address, uint8_t *data) {
	// Can only write to a 16 byte page boundary
	address &= 0xFFF0;
	
	// If we are writing page 0, change the reset vector
	// so that it jumps to the bootloader.
	if (_safeMode and address == 0) {
		data[0] = 0xFF;
		data[1] = 0xCB;
	}
	
	// If the address is past the application section don't write anything
	if (_safeMode and address >= 1024*6) {
		return;
	}
	
	eeprom_busy_wait();
	boot_spm_busy_wait();
	for (int i=0; i < SPM_PAGESIZE; i += 2) {
		uint16_t w = data[i] | (data[i+1] << 8);
		boot_page_fill(address+i, w);
	}
	boot_page_write(address);
	boot_spm_busy_wait();
}

