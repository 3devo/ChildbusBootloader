Attiny I²C bootloader
=====================
This is a bootloader intended for attiny devices, accessible through an
I²C bus.

It was developed by 3devo for their Filament Extruder to support a
system with a single master controller and one or more supporting slave
controllers.
 - At poweron, a slave starts the bootloader. This slave listens on the
   I²C bus for commands for the master. There is no timeout, so the
   bootloader will just wait if the master does not send any info.
 - The master detects the slave and uploads an application to it. This
   happens again on every boot (though the slave takes care to prevent a
   flash erase and write cycle when the data is identical).
 - The master tells the slave to start the application.

Since the attinies have no native bootloader support (the reset vector
always resets to address 0x0), the bootloader automatically overwrites
the reset vector with its own start address and saves the original reset
vector in a separate area of flash (just before the bootloader start).

This code only supports the attiny841 and attiny441 (only 841 was
tested) with TWI hardware. Some attempts have been made to also support
attiny24/44/84 with USI hardware, which worked (see the two-wire-usi
branch), but due to some of the polling required this was a bit less
robust.

The bootloader needs about 2k of flash.

License
-------
The bootloader is based on the bootloader written by Erin Tomson for the
Modulo board. Mostly the TWI implementation remains, most of the other
parts of the bootloader have been replaced.

All code and accompanying materials is licensed under the GPL, version
3. See the LICENSE file for the full license.

One exception is the protocol documentation, in the PROTOCOL.md file,
which has a more liberal license (see the file for the exact license).
