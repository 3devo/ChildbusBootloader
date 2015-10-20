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


// Flash page size is 16 bytes
void erasePage(uint16_t address) {
	uint8_t sreg = SREG;
	cli();
	
	eeprom_busy_wait();
	boot_page_erase(address);
	boot_spm_busy_wait();
	
	sei();
	SREG = sreg;
}

void readPage(uint8_t address, uint8_t *data) {
	for (int i=0; i < SPM_PAGESIZE; i++) {
		data[i] = pgm_read_byte(address+i);
	}
}

void writePage(uint16_t address, uint8_t *data) {
	uint8_t sreg = SREG;
	cli();
	
	eeprom_busy_wait();
	for (int i=0; i < SPM_PAGESIZE; i += 2) {
		uint16_t w = data[i] | (data[i+1] << 8);
		boot_page_fill(address+i, w);
	}
	boot_page_write(address);
	boot_spm_busy_wait();
	
	sei();
	SREG = sreg;
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



#define FUNCTION_EXIT_BOOTLOADER 100
#define FUNCTION_GET_BOOTLOADER_VERSION 101
#define FUNCTION_GET_NEXT_DEVICE_ID 102
#define FUNCTION_SET_DEVICE_ID 103
#define FUNCTION_GET_MCU_SIGNATURE 104
#define FUNCTION_READ_PAGE 105
#define FUNCTION_ERASE_PAGE 106
#define FUNCTION_WRITE_PAGE 107
#define FUNCTION_READ_EEPROM 108
#define FUNCTION_WRITE_EEPROM 110



void blExit() {
	
}


static uint16_t _deviceID = 0;

// The attiny841 has a 512 byte EEPROM section.
// The deviceID is at the end of the EEPROM section (address 510)
#define DEVICE_ID_ADDRESS ((uint16_t*)(510))

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

bool checkDeviceID(uint8_t *data) {
	return (getWord(data) == _deviceID);
}

#define BOOTLOADER_VERSION 1234

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	if (len < 1) {
		return 0;
	}
	
	switch (data[0]) {
		case FUNCTION_EXIT_BOOTLOADER:
			blExit();
			break;
		case FUNCTION_GET_BOOTLOADER_VERSION:
			if (len == 3 and checkDeviceID(data+1)) {
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
			if (len == 5 and checkDeviceID(data+1)) {
				_deviceID = getWord(data+3);
				storeDeviceID();
			}
			break;

		case FUNCTION_GET_MCU_SIGNATURE:
			if (len == 3 and checkDeviceID(data+1)) {
				data[0] = boot_signature_byte_get(0);
				data[1] = boot_signature_byte_get(2);
				data[2] = boot_signature_byte_get(4);
				return 3;
			}
			break;
		case FUNCTION_READ_PAGE:
			if (len == 5 and checkDeviceID(data+1)) {
				uint16_t address = getWord(data+3);
				
				readPage(address, data);
				return SPM_PAGESIZE;
			}
			break;
		case FUNCTION_ERASE_PAGE:
			if (len == 5 and checkDeviceID(data+1)) {
				uint16_t address = getWord(data+3);
				erasePage(address);
			}
			break;
		case FUNCTION_WRITE_PAGE:
			if (len == 5+SPM_PAGESIZE and checkDeviceID(data+1)) {
				uint16_t address = getWord(data+3);				
				writePage(address, data+5);
			}
			break;
		case FUNCTION_READ_EEPROM:
			if (len == 5 and checkDeviceID(data+1)) {
				
			}
			break;
		case FUNCTION_WRITE_EEPROM:
			break;
	}
	
	return 0;
}

uint8_t twiReadDataCallback(uint8_t *data, uint8_t maxLen) {
	return 0;
}


int main(void) 
{
#if 1
	// XXX -nostartfiles dosn't currently work
	// We are excluding the startup files (with the -nostartfiles linker option) so
	// we need to take care of a few things here.
	
	//asm volatile ( ".set __stack, %0" :: "i" (RAMEND) );
	SP = RAMEND;
	asm volatile ( "clr __zero_reg__" );
	
	// Initialize r1 to 0
	asm("eor r1,r1");
#endif

	volatile uint8_t sigByte = boot_signature_byte_get(4);

	loadDeviceID();
	TwoWireInit(9);
	
	uint32_t i=0;
	
	DDRA |= _BV(5);
	while (1) {
		i++;
		
		if (i > 95000) {
			PORTA |= _BV(5); 
		}
		if (i > 100000) {
			PORTA &= ~_BV(5);
			i = 0;
		}
		
		TwoWireUpdate();
	}
}