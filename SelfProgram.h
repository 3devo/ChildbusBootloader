/*
 * SelfProgram.h
 *
 * Created: 11/12/2015 10:23:28 AM
 *  Author: ekt
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

	void getSignature(uint8_t *data, int len);

	void readEEPROM(uint8_t *data, void *address, int eeLen);

	void writeEEPROM(uint8_t *data, void *address, int len);

	static void setLED(bool on);

	int getPageSize();

	void erasePage(uint16_t address);

	int readPage(uint8_t address, uint8_t *data);

	void writePage(uint16_t address, uint8_t *data);

private:
	uint16_t _deviceID;
	bool _safeMode;	
};

#endif /* SELFPROGRAM_H_ */