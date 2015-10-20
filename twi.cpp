/*
 * twi.cpp
 *
 * Created: 10/6/2015 4:39:31 PM
 *  Author: ekt
 */ 

#include "twi.h"
#include <avr/io.h>

uint8_t twiBuffer[TWI_BUFFER_SIZE];
uint8_t twiBufferPos = 0;

void twiInit() {
	TWSA = (9 << 1); // Listen on address 9
	TWSCRA |= _BV(TWSIE) | _BV(TWEN); // Enable the two-wire interface and set a flag when stop is received.
	twiBufferPos = 0;
}

uint8_t twiReadNextByte() {
	if (twiBufferPos >= TWI_BUFFER_SIZE) {
		return 0;
	}
	return twiBuffer[twiBufferPos++];
}

void twiWriteNextByte(uint8_t d) {
	if (twiBufferPos >= TWI_BUFFER_SIZE) {
		return;
	}
	twiBuffer[twiBufferPos] = d;
}

void twiAddressReceived(bool isRead) {
	if (!isRead) {
		twiBufferPos = 0;
	} else {
		twiBufferPos = twiReadDataCallback(twiBuffer, TWI_BUFFER_SIZE);
	}
}

void twiStopReceived(bool isRead) {
	if (!isRead) {
		// Handle received data
		twiWriteDataCallback(twiBuffer, twiBufferPos);
		twiBufferPos = 0;
	}
}

// Send the acknowledge action (either ack or nack) and execute a two wire command.
// If this is in response to an address byte, continue should be true.
// If this is in response to a data byte, continue should be falsed and this completes the transaction.
//#define twCmd(ack, continue) \
//	TWSCRB |= (bool(ack) << TWAA) | _BV(TWCMD1) | (bool(continue) << TWCMD0);

void twCmd(bool ack, bool complete=false) {
	if (ack) {
		TWSCRB &= _BV(TWAA);
		} else {
		TWSCRB |= _BV(TWAA);
	}
	
	TWSCRB |= _BV(TWCMD1) | (complete ? 0 : _BV(TWCMD0));
}

void twiUpdate() {
	volatile uint8_t status = TWSSRA;
	
	// Handle an address or stop interrupt
	volatile bool addressOrStopInterrupt = status & _BV(TWASIF);
	volatile bool isAddressNotStop = status & _BV(TWAS);
	volatile bool dataInterrupt = status & _BV(TWDIF);
	volatile bool isRead = status & _BV(TWDIR);
	
	if (addressOrStopInterrupt) {
		if (isAddressNotStop) {
			// Address matched. Must ACK the address to release bus hold and continue.
			twCmd(true, false);
			twiAddressReceived(isRead);
			} else {
			// Stop condition received. Must issue command to release bus hold and continue.
			twCmd(true, true);
			twiStopReceived(isRead);
		}
	}
	
	if (dataInterrupt) { // Data interrupt flag
		if (isRead) {
			TWSD = twiReadNextByte();
			} else {
			twiWriteNextByte(TWSD);
		}
	}
}



