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

// This is the Modulo bootloader for attiny841 based devices.
// The bootloader uses the following memory layout
//
//   0x0000 - 2 byte reset vector. Jumps to the bootloader section (0x1800)
//   0x17FE - 2 byte application trampoline. The bootloader will jump here when it exits.
//   0x1800 - 2048 byte bootloader
//
// The bootloader will write bytes 0x0002-0x17FF, protecting the reset vector and
// bootloader code. The only modification needed to make a program compatible with the
// bootloader is to insert a jump instruction at address 0x17FE to start the application.
//
// To view the disassembled bootloader, run:
//  /cygdrive/c/Program\ Files\ \(x86\)/Atmel/Atmel\ Toolchain/AVR8\ GCC/Native/3.4.1061/avr8-gnu-toolchain/bin/avr-objdump.exe -d Release/bootloader-attiny.elf


#include <avr/io.h>
#include "bootloader.h"

// This is a single instruction placed immediately before the bootloader.
// The application code will overwrite this instruction with a jump to to the start of the application.
void __attribute__((noinline)) __attribute__((naked)) __attribute__((section(".boot_trampoline"))) startApplication();
void startApplication() {
	asm("nop");
}

// Store the fuse bits in a separate section of the elf file.
// Note that fuse bits are inverted (0 enables the feature) so we must bitwise
// and the masks together.
FUSES =
{
	.low = FUSE_SUT_CKSEL4 & FUSE_SUT_CKSEL3 & FUSE_SUT_CKSEL2 & FUSE_SUT_CKSEL0,
	.high = FUSE_SPIEN & FUSE_EESAVE,
	.extended = FUSE_SELFPRGEN & FUSE_BODACT0 & FUSE_BODPD0
};

int main() {
	runBootloader();
	startApplication();
}
