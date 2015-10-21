/*
 * main.cpp
 *
 * Created: 10/21/2015 11:05:46 AM
 *  Author: ekt
 */ 


#include <avr/io.h>
#include "bootloader.h"

// This function is called by the bootloader to start the application.
// It will be linked at the same address as a normal, non-bootloaded program's
// reset vector. It must be in the ".application" section to ensure that it's
// linked at the correct address, and not inlined to ensure that the bootloader
// really calls it.
//void startAppliation() ;
// This is a single instruction placed at the end of the application section, immediately
// before the bootloader. In the application code, it should be a single instruction that
// jumps to the start of the application.

void __attribute__((noinline)) __attribute__((section(".boot_trampoline"))) __attribute__((naked)) startApplication() {
	asm("rjmp __init");
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
