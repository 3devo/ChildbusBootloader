/*
 * bootloader_attiny.cpp
 *
 * Created: 10/6/2015 12:27:29 PM
 *  Author: ekt
 */

#define SIGRD 5

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "TwoWire.h"
#include <string.h>


bool bootloaderSafeMode = true;

void writePage(uint16_t address, uint8_t *data);

// Flash page size is 16 bytes
void erasePage(uint16_t address) {
	// Can only write to a 64 byte 4 page boundary, so mask the lower 6 bits
	address &= 0xFFC0;
	
	// Bail if the address is past the application section.
	if (bootloaderSafeMode and address >= 1024*6) {
		return;
	}
	
	//eeprom_busy_wait();
	boot_page_erase_safe(address);
	//boot_spm_busy_wait();
	
	// If we just erased page 0, restore the reset vector in case
	// the bootloader gets interrupted before writing the page.
	// writePage() will change bytes 0 and 1.
	// All the other bytes in the page must remain erased (0xFF)
	if (bootloaderSafeMode and address == 0) {
		uint8_t data[SPM_PAGESIZE];
		for (int i=0; i < SPM_PAGESIZE; i++) {
			data[i] = 0xFF;
		}
		writePage(0, data);
	}
}

void readPage(uint8_t address, uint8_t *data) {
	// Can only read at a 16 byte page boundary
	address &= 0xFFF0;
	
	for (int i=0; i < SPM_PAGESIZE; i++) {
		data[i] = pgm_read_byte(address+i);
	}
}

void writePage(uint16_t address, uint8_t *data) {
	// Can only write to a 16 byte page boundary
	address &= 0xFFF0;
	
	// If we are writing page 0, change the reset vector
	// so that it jumps to the bootloader.
	if (bootloaderSafeMode and address == 0) {
		data[0] = 0xFF;
		data[1] = 0xCB;
	}
	
	// If the address is past the application section don't write anything
	if (bootloaderSafeMode and address >= 1024*6) {
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


// Commands
//

// GetNextDeviceID(previousID) -> uint16 nextID
// GetDeviceType() -> string deviceType
// ExitBootloader()

// SetDeviceType(string type)
// SetDeviceID(uint16_t newID)

// GetBootloaderVersion() -> uint16 version
// GetMCUSignature() -> uint32 signature
// ReadPage(uint16_t address) -> uint8[16]
// ErasePage(uint16_t address)
// WritePage(uint16_t address, uint8_t data[16])
// ReadEEPROM(uint8_t address, uint8_t len) -> uint8_t[]
// WriteEEPROM(uint8_t address, uint8_t data[], uint8_t len)



#define FUNCTION_GLOBAL_RESET 0
#define FUNCTION_GET_BOOTLOADER_VERSION 101
#define FUNCTION_GET_NEXT_DEVICE_ID 102
#define FUNCTION_SET_DEVICE_ID 103
#define FUNCTION_GET_MCU_SIGNATURE 104
#define FUNCTION_READ_PAGE 105
#define FUNCTION_ERASE_PAGE 106
#define FUNCTION_WRITE_PAGE 107
#define FUNCTION_READ_EEPROM 108
#define FUNCTION_WRITE_EEPROM 110
#define FUNCTION_SET_BOOTLOADER_SAFE_MODE 111

static uint16_t _deviceID = 0;
bool bootloaderRunning = true;

// The attiny841 has a 512 byte EEPROM section.
// The deviceID is at the end of the EEPROM section (address 510)
#define DEVICE_ID_ADDRESS ((uint16_t*)(476))

void loadDeviceID() {
	_deviceID = eeprom_read_word(DEVICE_ID_ADDRESS);
	// 0xFFFF is an invalid device ID. Default to deviceID=1
	if (_deviceID == 0xFFFF) {
		_deviceID = 1;
	}
}

void storeDeviceID() {
	eeprom_write_word(DEVICE_ID_ADDRESS, _deviceID);
}

uint16_t getWord(uint8_t *data) {
	return data[0] | (data[1] << 8);
}

int checkDeviceID(uint8_t *data) {
	return (getWord(data) == _deviceID);
}

#define BOOTLOADER_VERSION 1

static void setLED(bool on) {
	if (on) {
		PORTA |= _BV(5);
	} else {
		PORTA &= ~_BV(5);
	}
}

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	
	if (len < 1) {
		return 0;
	}
	
	switch (data[0]) {
		case FUNCTION_GLOBAL_RESET:
			bootloaderRunning = false;
			break;
		case FUNCTION_GET_BOOTLOADER_VERSION:
			if (len == 3 && checkDeviceID(data+1)) {
				// Return the device ID
				data[0] = BOOTLOADER_VERSION & 0xFF;
				data[1] = BOOTLOADER_VERSION >> 8;
				return 2;
			}
			break;
		case FUNCTION_GET_NEXT_DEVICE_ID:
			if (len >= 3) {
				uint16_t previousID = getWord(data+1);
				if (previousID < _deviceID) {
					// Return the device ID
					data[0] = _deviceID & 0xFF;
					data[1] = _deviceID >> 8;
					return 2;
				}
			}
			break;
		case FUNCTION_SET_DEVICE_ID:
			if (len == 5 && checkDeviceID(data+1)) {
				_deviceID = getWord(data+3);
				storeDeviceID();
			}
			break;

		case FUNCTION_GET_MCU_SIGNATURE:
			if (len == 3 && checkDeviceID(data+1)) {
				data[0] = boot_signature_byte_get(0);
				data[1] = boot_signature_byte_get(2);
				data[2] = boot_signature_byte_get(4);
				return 3;
			}
			break;
		case FUNCTION_READ_PAGE:
			if (len == 5 && checkDeviceID(data+1)) {
				setLED(false);
				uint16_t address = getWord(data+3);
				
				readPage(address, data);
				setLED(true);
				return SPM_PAGESIZE;
			}
			break;
		case FUNCTION_ERASE_PAGE:
			if (len == 5 && checkDeviceID(data+1)) {
				setLED(false);
							
				uint16_t address = getWord(data+3);
				erasePage(address);
				
				setLED(true);
			}
			break;
		case FUNCTION_WRITE_PAGE:
			if (len == 5+SPM_PAGESIZE && checkDeviceID(data+1)) {
				setLED(false);
				
				uint16_t address = getWord(data+3);				
				writePage(address, data+5);
				
				setLED(true);
			}
			break;
		case FUNCTION_READ_EEPROM:
			if (len == 5 && checkDeviceID(data+1)) {
				
			}
			break;
		case FUNCTION_WRITE_EEPROM:
			break;
		case FUNCTION_SET_BOOTLOADER_SAFE_MODE:
			if (len == 4 && checkDeviceID(data+1)) {
				bootloaderSafeMode = data[3];
			}
			break;
	}
	
	return 0;
}

extern "C" {
	void runBootloader() {
		
		loadDeviceID();
		TwoWireInit(false /*useInterrupts*/);
	
		DDRA |= _BV(5);
		PORTA |= _BV(5);

		while (bootloaderRunning) {
			TwoWireUpdate();
		}
	}
};
