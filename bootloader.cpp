/*
 * bootloader_attiny.cpp
 *
 * Created: 10/6/2015 12:27:29 PM
 *  Author: ekt
 */



#include <string.h>

#include "TwoWire.h"
#include "SelfProgram.h"

#define BOOTLOADER_VERSION 1

void writePage(uint16_t address, uint8_t *data);

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

#define FUNCTION_EXIT_BOOTLOADER 100
#define FUNCTION_GET_BOOTLOADER_VERSION 101
#define FUNCTION_GET_NEXT_DEVICE_ID 102
#define FUNCTION_SET_DEVICE_ID 103
#define FUNCTION_GET_MCU_SIGNATURE 104
#define FUNCTION_READ_PAGE 105
#define FUNCTION_ERASE_PAGE 106
#define FUNCTION_WRITE_PAGE 107
#define FUNCTION_READ_EEPROM 108
#define FUNCTION_WRITE_EEPROM 109
#define FUNCTION_SET_BOOTLOADER_SAFE_MODE 110


SelfProgram selfProgram;
bool bootloaderRunning = true;

uint16_t getWord(uint8_t *data) {
	return data[0] | (data[1] << 8);
}

int checkDeviceID(uint8_t *data) {
	return (getWord(data) == selfProgram.getDeviceID());
}

int TwoWireCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	
	if (len < 1) {
		return 0;
	}
	
	switch (data[0]) {
		case FUNCTION_GLOBAL_RESET:
		case FUNCTION_EXIT_BOOTLOADER:
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
				uint16_t deviceID = selfProgram.getDeviceID();
				if (previousID < deviceID) {
					// Return the device ID
					data[0] = deviceID & 0xFF;
					data[1] = deviceID >> 8;
					return 2;
				}
			}
			break;
		case FUNCTION_SET_DEVICE_ID:
			if (len == 5 && checkDeviceID(data+1)) {
				selfProgram.storeDeviceID(getWord(data+3));
			}
			break;

		case FUNCTION_GET_MCU_SIGNATURE:
			if (len == 3 && checkDeviceID(data+1)) {
				selfProgram.getSignature(data, 3);
				return 3;
			}
			break;
		case FUNCTION_READ_PAGE:
			if (len == 5 && checkDeviceID(data+1)) {
				uint16_t address = getWord(data+3);
				return selfProgram.readPage(address, data);
			}
			break;
		case FUNCTION_ERASE_PAGE:
			if (len == 5 && checkDeviceID(data+1)) {
				selfProgram.setLED(false);
							
				uint16_t address = getWord(data+3);
				selfProgram.erasePage(address);
				
				selfProgram.setLED(true);
			}
			break;
		case FUNCTION_WRITE_PAGE:
			if (len == 5+selfProgram.getPageSize() && checkDeviceID(data+1)) {
				selfProgram.setLED(false);
				
				uint16_t address = getWord(data+3);
				selfProgram.writePage(address, data+5);
				
				selfProgram.setLED(true);
			}
			break;
		case FUNCTION_READ_EEPROM:
			if (len == 6 && checkDeviceID(data+1)) {
				uint16_t address = getWord(data+3);
				uint16_t eeLen = data[5];
				
				if (eeLen > maxLen) {
					return 0;
				}
				
				selfProgram.readEEPROM(data, (void*)address, eeLen);
			
				return len;
			}
			break;
		case FUNCTION_WRITE_EEPROM:
			if (len >= 5  && checkDeviceID(data+1)) {
				volatile uint16_t address = getWord(data+3);
				
				selfProgram.writeEEPROM(data+5, (void*)address, len-5);
			}
			break;
		case FUNCTION_SET_BOOTLOADER_SAFE_MODE:
			if (len == 4 && checkDeviceID(data+1)) {
				selfProgram.setSafeMode(data[3]);
			}
			break;
	}
	
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
