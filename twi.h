/*
 * twi.h
 *
 * Created: 10/6/2015 4:39:19 PM
 *  Author: ekt
 */ 


#ifndef TWI_H_
#define TWI_H_

#include <inttypes.h>

#define TWI_BUFFER_SIZE 32
extern uint8_t twiBuffer[TWI_BUFFER_SIZE];
extern uint8_t twiBufferPos;

// Called by the twi library when data is received
void twiWriteDataCallback(uint8_t *data, uint8_t len);

// Called by the twi library when data is requested
uint8_t twiReadDataCallback(uint8_t *data, uint8_t maxLen);

void twiUpdate();
void twiInit();

#endif /* TWI_H_ */