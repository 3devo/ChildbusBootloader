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

//TODO
//#if defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
//
/*
 - Some global variables were renamed to match TwoWire841.cpp
 - Support for a broadcast address is added
 - Separate RX and TX buffers were merged into one buffer
 - Interrupt-disabling was modified to restore SREG instead of
   unconditionally enabling interrupts.
 - Interrupt-flag polling was added to support interrupt-less operation.
   This needed an extra of_state_wait_for_start, to prevent polling the
   overflow interrupt flag in the start condition.
 - When a stop condition is detected, always call twi_reset (previously
   it was not called when data was received, but also not when data was
   still *being* received).
 - Only call the callback when data was received, not after a transmit.
 - ss_state was removed, as it was not really used anymore.
 - Code for sleeping and statistics was left out.
 - Poll the data collision flag to allow multiple slaves to transmit at
   the same time.
 - Reorder initialization in twi_reset() to prevent unneeded glitches
   and remove unneeded bitflips.
*/

#include "TwoWire.h"
#include "UsiDevices.h"
#include <avr/interrupt.h>

#if defined(USI_OVERFLOW_VECTOR)

static volatile uint8_t _deviceAddress = 0;
static const uint8_t broadcastAddress = 9;

enum overflow_state_t
{
        of_state_wait_for_start,
        of_state_check_address,
        of_state_send_data,
        of_state_request_ack,
        of_state_check_ack,
        of_state_receive_data,
        of_state_store_data_and_send_ack
};

static uint8_t of_state;
static bool use_interrupts;

#define TWI_BUFFER_SIZE 32
static uint8_t twiBuffer[TWI_BUFFER_SIZE];
static uint8_t twiBufferLen = 0;
static uint8_t twiReadPos = 0;
static uint8_t twiAddress = 0;

static void set_sda_to_input(void)
{
        DDR_USI &= ~_BV(PORT_USI_SDA);
}

static void set_sda_to_output(void)
{
        DDR_USI |= _BV(PORT_USI_SDA);
}

static uint8_t sda_is_output(void)
{
        return DDR_USI & _BV(PORT_USI_SDA);
}

static inline void set_scl_to_input(void)
{
        DDR_USI &= ~_BV(PORT_USI_SCL);
}

static inline void set_scl_to_output(void)
{
        DDR_USI |= _BV(PORT_USI_SCL);
}

static inline void set_sda_low(void)
{
        PORT_USI &= ~_BV(PORT_USI_SDA);
}

static inline void set_sda_high(void)
{
        PORT_USI |= _BV(PORT_USI_SDA);
}

static inline void set_scl_low(void)
{
        PORT_USI &= ~_BV(PORT_USI_SCL);
}

static inline void set_scl_high(void)
{
        PORT_USI |= _BV(PORT_USI_SCL);
}

static inline void twi_reset_state(void)
{
        of_state = of_state_wait_for_start;
        USISR =
                (1     << USISIF)      |    // clear start condition flag
                (1     << USIOIF)      |    // clear overflow condition flag
                (0     << USIPF)       |    // !clear stop condition flag
                (1     << USIDC)       |    // clear arbitration error flag
                (0x00  << USICNT0);         // set counter to "8" bits

        USICR =
                (use_interrupts << USISIE) |                    // enable start condition interrupt
                (0 << USIOIE) |                                 // !enable overflow interrupt
                (1 << USIWM1) | (0 << USIWM0) |                 // set usi in two-wire mode, disable bit counter overflow hold
                (1 << USICS1) | (0 << USICS0) | (0 << USICLK) | // shift register clock source = external, positive edge, 4-bit counter source = external, both edges
                (0 << USITC);                                   // don't toggle clock-port pin
}

static void twi_reset(void)
{
        // Enable or reset the USI module
        twi_reset_state();

        // With the USI module turned on, the scl and sda pins turn into
        // open-collector pins when in output mode (LOW or floating).
        // Additionally, the pins are LOW when either the USI module
        // wants them low *or* the PORT register has them set LOW. Since
        // we want the USI module to completely control the pins, make
        // sure the PORT register is set to HIGH.
        set_sda_high();
        set_scl_high();

        // The SCL pin is always output, which allows the USI module to
        // do clock stretching (it doesn't touch SCL otherwise). SDA is
        // normally input, but switched to output when sending a NAK or
        // data.
        set_sda_to_input();
        set_scl_to_output();

}

void TwoWireInit(bool useInterrupts) {
// If USI_ON_PORT_A is defined, map the USI pins to non-standard pins on
// PORTA using the USIPP register
#if defined(USIPP)
#if  defined(USI_ON_PORT_A)
        USIPP |= _BV(USIPOS);
#else
        USIPP &= ~_BV(USIPOS);
# endif
#endif

        use_interrupts = useInterrupts;

        twi_reset();
}

void TwoWireSetDeviceAddress(uint8_t address) {
	_deviceAddress = address;
}

uint8_t TwoWireGetDeviceAddress() {
	return _deviceAddress;
}

ISR(USI_START_vect)
{
        set_sda_to_input();

        // wait for SCL to go low to ensure the start condition has completed (the
        // start detector will hold SCL low) - if a stop condition arises then leave
        // the interrupt to prevent waiting forever - don't use USISR to test for stop
        // condition as in Application Note AVR312 because the stop condition Flag is
        // going to be set from the last TWI sequence

        while(!(PIN_USI & _BV(PIN_USI_SDA)) &&
                        (PIN_USI & _BV(PIN_USI_SCL)))

        // possible combinations
        //      sda = low       scl = low               break   start condition
        //      sda = low       scl = high              loop
        //      sda = high      scl = low               break   stop condition
        //      sda = high      scl = high              break   stop condition

        if((PIN_USI & _BV(PIN_USI_SDA)))        // stop condition
        {
                twi_reset();
                return;
        }

        of_state = of_state_check_address;

        USIDR = 0xff;

        USICR =
                (use_interrupts << USISIE) |                    // enable start condition interrupt
                (use_interrupts << USIOIE) |                   // enable overflow interrupt
                (1 << USIWM1) | (1 << USIWM0) |                 // set usi in two-wire mode, enable bit counter overflow hold
                (1 << USICS1) | (0 << USICS0) | (0 << USICLK) | // shift register clock source = external, positive edge, 4-bit counter source = external, both edges
                (0 << USITC);                                   // don't toggle clock-port pin

        USISR =
                (1      << USISIF)      |  // clear start condition flag
                (1      << USIOIF)      |  // clear overflow condition flag
                (0      << USIPF)       |  // !clear stop condition flag
                (1      << USIDC)       |  // clear arbitration error flag
                (0x00   << USICNT0);       // set counter to "8" bits
}

ISR(USI_OVERFLOW_VECTOR)
{
        // bit shift register overflow condition occured
        // scl forced low until overflow condition is cleared!

        uint8_t data        = USIDR;
        uint8_t set_counter = 0x00; // send 8 bits (16 edges)

again:
        switch(of_state)
        {
                // start condition occured and succeed
                // check address, if not OK, reset usi
                // note: not using general call address
                case(of_state_check_address):
                {
                        uint8_t direction;
                        direction       = data & 0x01;
                        twiAddress      = (data & 0xfe) >> 1;

                        bool addressed = (_deviceAddress && twiAddress == _deviceAddress);
                        bool broadcast = (broadcastAddress && twiAddress == broadcastAddress);

                        // Reply when we are adressed (either directly
                        // or through the broadcast address) and this is
                        // a write request (command) or we have a reply
                        // to send.
                        if((addressed || broadcast) && (!direction || twiBufferLen > 0))
                        {
                                if(direction) // read request from master
                                        of_state = of_state_send_data;
                                else          // write request from master
                                        of_state = of_state_receive_data;

                                USIDR      = 0x00;
                                set_counter = 0x0e;  // send 1 bit (2 edges)
                                set_sda_to_output(); // initiate send ack
                        }
                        else
                        {
                                // Not addressed
                                USIDR       = 0x00;
                                set_counter = 0x00;
                                twi_reset_state();
                        }

                        break;
                }

                // process read request from master
                case(of_state_send_data):
                {
                        of_state = of_state_request_ack;

                        if(twiReadPos < twiBufferLen)
                                USIDR = twiBuffer[twiReadPos++];
                        else
                                USIDR = 0x00;  // no more data, but cannot send "nothing" or "nak"

                        set_counter = 0x00;
                        set_sda_to_output();   // initiate send data

                        break;
                }

                // data sent to master, request ack (or nack) from master
                case(of_state_request_ack):
                {
                        of_state = of_state_check_ack;

                        USIDR       = 0x00;
                        set_counter = 0x0e;  //      receive 1 bit (2 edges)
                        set_sda_to_input();  //      initiate receive ack

                        break;
                }

                // ack/nack from master received
                case(of_state_check_ack):
                {
                        if(data)        // if NACK, the master does not want more data
                        {
                                of_state = of_state_check_address;
                                set_counter = 0x00;
                                twi_reset();
                        }
                        else
                        {
                                of_state = of_state_send_data;
                                goto again; // from here we just drop straight into state_send_data
                        }                   // don't wait for another overflow interrupt

                        break;
                }

                // process write request from master
                case(of_state_receive_data):
                {
                        of_state = of_state_store_data_and_send_ack;

                        set_counter = 0x00; // receive 8 bits (16 edges)
                        set_sda_to_input(); // initiate receive data

                        break;
                }

                // data received from master, store it and wait for more data
                case(of_state_store_data_and_send_ack):
                {
                        of_state = of_state_receive_data;

                        if(twiBufferLen < sizeof(twiBuffer))
                                twiBuffer[twiBufferLen++] = data;

                        USIDR       = 0x00;
                        set_counter = 0x0e;  // send 1 bit (2 edges)
                        set_sda_to_output(); // initiate send ack

                        break;
                }
        }

        USISR =
                (0 << USISIF) |               // don't clear start condition flag
                (1 << USIOIF) |               // clear overflow condition flag
                (0 << USIPF)  |               // don't clear stop condition flag
                (1 << USIDC)  |               // clear arbitration error flag
                (set_counter    << USICNT0);  // set counter to 8 or 1 bits
}

void TwoWireUpdate() {
        if(!use_interrupts) {
                if(USISR & _BV(USISIF))
                        USI_START_vect();

                if(of_state != of_state_wait_for_start && (USISR & _BV(USIOIF)))
                        USI_OVERFLOW_VECTOR();
        }

        if(USISR & _BV(USIPF))
        {
                uint8_t sreg = SREG;
                if (use_interrupts)
                        cli();

                USISR |= _BV(USIPF);    // clear stop condition flag

                bool data_received = (of_state == of_state_store_data_and_send_ack);
                twi_reset();

                SREG = sreg;


                if(data_received)
                {
                        twiBufferLen = TwoWireCallback(twiAddress, twiBuffer, twiBufferLen, sizeof(twiBuffer));
                        twiReadPos = 0;
                }
        }

        // When a data collision happens (we write a 1 but read a 0, so
        // another slave is also sending), back off and wait for another
        // start condition. This allows for multiple slaves to respond
        // to a broadcast address, with the lowest value reply winning.
        // Checking for collision is a bit delicate, since the USIDC bit
        // gets updated immediately (no need to explicitly clear it, but
        // if you miss it, it's gone), regardless of the current state
        // of things.
        // Collision should be checked when:
        //  - SDA is an output, and
        //  - The clock is high (since when the clock is low, the data
        //    bit might be changing, which could take a while due to
        //    capacitance). The clock is high when the counter is odd.
        //
        // There is a potential race condition, where the clock is high
        // when you check the counter, but then the clock drops before
        // you can check USIDC. If the next bit shifted from USIDR is
        // different, it might take a while before the pin reads back
        // the same value that is written (especially on a low->high
        // transition) and during that time USIDC might be set. To
        // catch this case, the counter is checked a second time after
        // checking USIDC.
        //
        // Note that since this is polled and the USIDC bit is not
        // sticky, this only works at lower I2c speeds (otherwise the
        // collision is missed). Using an 8Mhz CPU clock, this seems to
        // work up to about 75Khz (90Khz when use_interrupts is made a
        // constant false).
        if (sda_is_output() && ((USISR >> USICNT0) & 0x1) && USISR & _BV(USIDC) && ((USISR >> USICNT0) & 0x1)) {
                // Prevent further output to SDA ASAP
                set_sda_to_input();

                // Then reset the state
                PINA = (1 << PA1);
                uint8_t sreg = SREG;
                if (use_interrupts)
                        cli();

                twi_reset();

                SREG = sreg;
        }
}

#endif
