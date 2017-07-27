#include <stdint.h>

#pragma once

struct Status {
  enum {
    COMMAND_OK            = 0x00,
    COMMAND_FAILED        = 0x01,
    COMMAND_NOT_SUPPORTED = 0x02,
    INVALID_TRANSFER      = 0x03,
    INVALID_CRC           = 0x04,
    INVALID_ARGUMENTS     = 0x05,
  };
};

struct Commands {
  enum {
    GET_PROTOCOL_VERSION  = 0x00,
    SET_I2C_ADDRESS       = 0x01,
    POWER_UP_DISPLAY      = 0x02,
    GET_HARDWARE_INFO     = 0x03,
    START_APPLICATION     = 0x04,
    WRITE_FLASH           = 0x05,
    FINALIZE_FLASH        = 0x06,
    READ_FLASH            = 0x07,
    END_OF_COMMANDS
  };
};

struct GeneralCallCommands {
  static const uint8_t RESET = 0x06;
  static const uint8_t RESET_ADDRESS = 0x04;
};

static const uint8_t GENERAL_CALL_ADDRESS = 0;
static const uint8_t FIRST_ADDRESS = 8;
static const uint8_t LAST_ADDRESS = 15;
static const uint8_t MAX_MSG_LEN = 32;

// Expected values
static const uint16_t PROTOCOL_VERSION = 0x0100;
static const uint8_t HARDWARE_TYPE = 0x01;
static const uint8_t HARDWARE_REVISION = 0x01;
static const uint8_t BOOTLOADER_VERSION = 0x01;
static const uint16_t AVAILABLE_FLASH_SIZE = 8192-2048-2;

// As returned by POWER_UP_DISPLAY
static const uint8_t DISPLAY_CONTROLLER_TYPE = 0x01;
static const uint8_t DISPLAY_I2C_ADDRESS = 0x3C;
