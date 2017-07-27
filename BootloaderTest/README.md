# Bootloader test suite
This Arduino sketch runs various tests on the bootloader. It is intended
to run on a 3devo devoboard, atmega2560 or other avr connected to the an
attiny running the bootloader through I²C.

It depends on two libraries:
 - https://github.com/mmurdoch/arduinounit
 - https://github.com/stevemarple/SoftWire

At the time of writing, both libraries need to modified to work,
hopefully these changes can be merged upstream at a later moment.

