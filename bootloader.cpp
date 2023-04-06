/*
 * Copyright (C) 2017 3devo (http://www.3devo.eu)
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

#include <string.h>
#if defined(__AVR__)
#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#elif defined(STM32)
#include <libopencm3/stm32/desig.h>
#endif
#include <stdio.h>

#include "Config.h"
#include "Bus.h"
#include "BaseProtocol.h"
#include "SelfProgram.h"
#include "bootloader.h"

// Make boot_signature_byte_get work on ATtiny841, until this is merged:
// https://savannah.nongnu.org/patch/index.php?9437
#if !defined(SIGRD) && defined(RSIG)
#define SIGRD RSIG
#endif

// On non-AVR architectures, pgm_read_* is not needed (constants in
// flash can be read directly)
#if !defined(PROGMEM)
	#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
	#define pgm_read_word(addr) (*(const uint16_t *)(addr))
	#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#endif

struct Commands {
	// See also ProtocolCommands in BaseProtocol.h
	static const uint8_t POWER_UP_DISPLAY      = 0x02;
	static const uint8_t GET_HARDWARE_INFO     = 0x03;
	static const uint8_t GET_SERIAL_NUMBER     = 0x04;
	static const uint8_t START_APPLICATION     = 0x05;
	static const uint8_t WRITE_FLASH           = 0x06;
	static const uint8_t FINALIZE_FLASH        = 0x07;
	static const uint8_t READ_FLASH            = 0x08;
	static const uint8_t GET_HARDWARE_REVISION = 0x09;
	static const uint8_t GET_NUM_CHILDREN      = 0x0a;
	static const uint8_t SET_CHILD_SELECT      = 0x0b;
	static const uint8_t GET_EXTRA_INFO        = 0x0d;
	static const uint8_t READ_BOARD_INFO       = 0x0e;
};

constexpr const uint8_t MAX_EXTRA_INFO = 16;

volatile bool bootloaderExit = false;

// Note that we must buffer a full erase page size (not smaller), since
// we must know at the start of an erase page whether any byte in the
// entire page is changed to decide whether or not to erase.
static uint8_t writeBuffer[FLASH_ERASE_SIZE];
static uint16_t nextWriteAddress = 0;

// This is a placeholder in flash, that should be replaced with the
// actual board info contents while flashing the bootloader.
// The section is explicitly set, to force this into flash (on AVR) and
// put it into a fixed location (to make it easy to replace when
// flashing). The contents of this variable is also omitted from the
// resulting elf file, otherwise (on STM32) it cannot be overwritten
// with the actual board info without doing another erase.
// This is volatile to prevent the compiler from optimizing against the
// current value (implicit zeroes) and force actually reading the value
// at runtime.
constexpr const uint8_t volatile BOARD_INFO[BOARD_INFO_SIZE] __attribute__((__section__(".board_info"))) = {};

// This defines the expected board info structure. Only a couple of fields are
// used (for getting values for older commands supported for compatibility),
// but for simplicity this just defines the entire struct.
struct BoardInfoV2 {
    uint32_t signature;
    uint8_t block_version_minor;
    uint8_t block_version_major;
    uint16_t design_files_date;
    uint16_t board_assembly_date;
    uint16_t board_flash_date;
    uint32_t design_files_vcs_hash;
    char batch_code[16];
    uint8_t reserved;
    uint8_t manufacturer_id;
    uint16_t board_number;
    uint32_t component_variations;
    uint8_t current_board_version;
    uint8_t compatible_board_version;
    uint8_t rfu[20];
    uint16_t crc;
};
static_assert(sizeof(BoardInfoV2) == BOARD_INFO_SIZE, "Unexpected board info size");

// These are loaded from the board info at runtime and used by the
// legacy information retrieval commands for older implementations.
static uint8_t compatible_board_version;
static uint8_t current_board_version;
#if defined(BOARD_TYPE_interfaceboard)
	static uint8_t extra_info[] = {0};
#endif


// Helper function that is declared but not defined, to allow
// semi-static assertions (where input to a check is not really const,
// but can be derived by the optimizer, so if the check passes, the call
// to this function is optimized away, and if not, produces a linker
// error).
void compiletime_check_failed();

static bool equalToFlash(uint16_t address, uint16_t len) {
	uint16_t offset = 0;
	while (len > 0) {
		if (writeBuffer[offset] != SelfProgram::readByte(FLASH_APP_OFFSET + address + offset))
			return false;
		--len;
		++offset;
	}
	return true;
}


static uint8_t commitToFlash(uint16_t address, uint16_t len) {
	// If nothing needs to be changed, then don't
	if (equalToFlash(address, len))
		return 0;

	uint16_t offset = 0;
	while (len > 0) {
		uint16_t pageLen = len < FLASH_WRITE_SIZE ? len : FLASH_WRITE_SIZE;
		uint8_t err = SelfProgram::writePage(FLASH_APP_OFFSET + address + offset, &writeBuffer[offset], pageLen);
		if (err)
			return err;
		len -= pageLen;
		offset += pageLen;
	}
	return 0;
}

static cmd_result handleWriteFlash(uint16_t address, uint8_t *data, uint16_t len, uint8_t *dataout) {
	if (address == 0)
		nextWriteAddress = 0;

	// Only consecutive writes are supported
	if (address != nextWriteAddress)
		return cmd_result(Status::INVALID_ARGUMENTS);

	nextWriteAddress += len;
	while (address < nextWriteAddress) {
		writeBuffer[address % sizeof(writeBuffer)] = *data;
		++data;
		++address;

		if (address % sizeof(writeBuffer) == 0) {
			uint8_t err = commitToFlash(address - sizeof(writeBuffer), sizeof(writeBuffer));
			if (err) {
				dataout[0] = err;
				return cmd_result(Status::COMMAND_FAILED, 1);
			}
		}
	}

	return cmd_ok();
}

#ifdef HAVE_DISPLAY
void displayOn() {
	// This pin has a pullup to 3v3, so the display comes out of
	// reset as soon as the 3v3 is powered up. To prevent that, pull
	// it low now.
	PIN_DISPLAY_RESET.write(0);

	// Reset sequence for the display according to datasheet: Enable
	// 3v3 logic supply, then release the reset, then powerup the
	// boost converter for LED power. This is a lot slower than
	// possible according to the datasheet.
	PIN_3V3_ENABLE.write(1);

        _delay_ms(1);
	// Switch to input to let external 3v3 pullup work instead of
	// making it high (which would be 5v);
	PIN_DISPLAY_RESET.hiz();

        _delay_ms(1);
	PIN_BOOST_ENABLE.write(1);

        _delay_ms(5);
}
#endif

cmd_result processCommand(uint8_t cmd, uint8_t *datain, uint8_t len, uint8_t *dataout, uint8_t maxLen) {
	if (maxLen < 5)
		compiletime_check_failed();

	// Preloading these bytes into a variable allows gcc to generate
	// slightly smaller code...
	uint8_t datain0 = datain[0], datain1 = datain[1], datain2 = datain[2];

	switch (cmd) {
		#ifdef HAVE_DISPLAY
		case Commands::POWER_UP_DISPLAY:
			displayOn();
			dataout[0] = DISPLAY_CONTROLLER_TYPE;
			return cmd_ok(1);
		#endif

		case Commands::GET_HARDWARE_INFO:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			dataout[0] = INFO_HW_TYPE;
			dataout[1] = compatible_board_version;
			dataout[2] = BL_VERSION;
			// Available flash size is up to startApplication.
			// Convert from words to bytes.
			uint16_t size = SelfProgram::applicationSize;
			dataout[3] = size >> 8;
			dataout[4] = size;
			return cmd_ok(5);
		}
		case Commands::GET_HARDWARE_REVISION:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			dataout[0] = current_board_version;
			return cmd_ok(1);
		}
		case Commands::GET_SERIAL_NUMBER:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			#if defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
			// These are offsets into the device signature imprint table, which
			// store the parts of the serial number (lot number, wafer number, x/y
			// coordinates).
			static const uint8_t PROGMEM serial_offset[] = {0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17};

			if (maxLen < sizeof(serial_offset))
				compiletime_check_failed();

			for (uint8_t i = 0; i < sizeof(serial_offset); ++i)
				dataout[i] = boot_signature_byte_get(pgm_read_byte(&serial_offset[i]));
			return cmd_ok(sizeof(serial_offset));
			#elif defined(STM32)
			const size_t id_size = 12;
			if (maxLen < id_size)
				compiletime_check_failed();

			// This access the id bytes directly rather than
			// using the desig_get_unique_id libopencm3
			// function, in order to use byte addressing
			// (word addressing requires dataout to be
			// aligned) and so we can reverse the bytes
			// (rather than the words) so the result has all
			// bytes in big endian order.
			// Note that that G030 does not document these
			// bytes, but they seem to be available
			// regardless (G030 seems to use the same core
			// die as G031)
			for (uint8_t i = 0; i < id_size; ++i)
				dataout[i] = ((uint8_t*)DESIG_UNIQUE_ID_BASE)[id_size - i - 1];

			return cmd_ok(id_size);
			#else
			return cmd_result(Status::COMMAND_NOT_SUPPORTED);
			#endif
		}
		case Commands::START_APPLICATION:
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			bootloaderExit = true;
			// This is probably never sent
			return cmd_ok();

		case Commands::WRITE_FLASH:
		{
			if (len < 2)
				return cmd_result(Status::INVALID_ARGUMENTS);

			uint16_t address = datain0 << 8 | datain1;
			return handleWriteFlash(address, datain + 2, len - 2, dataout);
		}
		case Commands::FINALIZE_FLASH:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			uint16_t pageAddress = nextWriteAddress & ~(sizeof(writeBuffer) - 1);
			uint8_t err = commitToFlash(pageAddress, nextWriteAddress - pageAddress);
			if (err) {
				dataout[0] = err;
				return cmd_result(Status::COMMAND_FAILED, 1);
			} else {
				dataout[0] = SelfProgram::eraseCount;
				SelfProgram::eraseCount = 0;
				return cmd_ok(1);
			}
		}
		case Commands::READ_FLASH:
		case Commands::READ_BOARD_INFO:
		{
			if (len != 3)
				return cmd_result(Status::INVALID_ARGUMENTS);

			uint16_t address = datain0 << 8 | datain1;
			uint8_t len = datain2;

			if (len > maxLen)
				return cmd_result(Status::INVALID_ARGUMENTS);

			if (cmd == Commands::READ_BOARD_INFO) {
				if (address >= sizeof(BOARD_INFO))
					len = 0;
				else if (len > sizeof(BOARD_INFO) - address)
					len = sizeof(BOARD_INFO) - address;
				address += reinterpret_cast<uintptr_t>(&BOARD_INFO);
			} else {
				address += FLASH_APP_OFFSET;
			}

			SelfProgram::readFlash(address, dataout, len);
			return cmd_ok(len);
		}
		#if defined(USE_CHILD_SELECT)
		case Commands::GET_NUM_CHILDREN:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			dataout[0] = NUM_CHILDREN;
			return cmd_ok(1);
		}
		case Commands::SET_CHILD_SELECT:
		{
			if (len != 2)
				return cmd_result(Status::INVALID_ARGUMENTS);

			uint8_t index = datain0;
			uint8_t state = datain1;

			if (index >= NUM_CHILDREN || state > 1)
				return cmd_result(Status::INVALID_ARGUMENTS);

			if (state)
				CHILDREN_SELECT_PINS[index].write(0);
			else
				CHILDREN_SELECT_PINS[index].hiz();

			return cmd_ok(0);
		}
		#endif // defined(USE_CHILD_SELECT)
		#if defined(BOARD_TYPE_interfaceboard)
		case Commands::GET_EXTRA_INFO:
		{
			if (len != 0)
				return cmd_result(Status::INVALID_ARGUMENTS);

			static_assert(sizeof(extra_info) <= MAX_EXTRA_INFO, "More EXTRA_INFO than protocol allows");

			if (sizeof(extra_info) > maxLen)
				compiletime_check_failed();

			memcpy(dataout, extra_info, sizeof(extra_info));
			return cmd_ok(sizeof(extra_info));
		}
		#endif // defined(EXTRA_INFO)

		default:
			return cmd_result(Status::COMMAND_NOT_SUPPORTED);
	}
}

extern "C" {
	void runBootloader() {
		ClockInit();
		BusInit();

		auto board_info = reinterpret_cast<const volatile BoardInfoV2*>(&BOARD_INFO);
		uint32_t signature = pgm_read_dword(&board_info->signature);
		uint8_t block_version = pgm_read_word(&board_info->block_version_major);
		if (signature == BOARD_INFO_SIGNATURE && block_version == BOARD_INFO_MAJOR_VERSION) {
			current_board_version = pgm_read_byte(&board_info->current_board_version);
			compatible_board_version = pgm_read_byte(&board_info->compatible_board_version);
			#if defined(BOARD_TYPE_interfaceboard)
			// This loads a byte instead of the full dword,
			// since we only need the LSB (and this produces
			// smaller code)
			extra_info[0] = pgm_read_byte(&board_info->component_variations) & 0xf;
			#endif
		} else {
			// If no valid board info is found, these will
			// cause the mainboard to refuse operation
			// (since there is no way to print a meaningful
			// error). These could also have been the
			// default values for these variables, but now
			// there are no global variables with non-zero
			// values, so no need for
			// a copy-data-from-flash-to-ram startup loop,
			// which saves 12 bytes on attiny.
			current_board_version = 0xff;
			compatible_board_version = 0xff;
		}


		while (!bootloaderExit) {
			#if !defined(BUS_USE_INTERRUPTS)
			BusUpdate();
			#endif // defined(BUS_USE_INTERRUPTS)
		}

		BusDeinit();
		ClockDeinit();
	}
}
