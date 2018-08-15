# Makefile to compile bootloader-attiny
#
# Copyright 2017 Matthijs Kooijman <matthijs@stdin.nl>
#
# Permission is hereby granted, free of charge, to anyone obtaining a
# copy of this document to do whatever they want with them without any
# restriction, including, but not limited to, copying, modification and
# redistribution.
#
# NO WARRANTY OF ANY KIND IS PROVIDED.
#
# To compile, just make sure that avr-gcc and friends are in your path
# and type "make".

PROTOCOL_VERSION = 0x0100

PRG            = bootloader
CPPSRC         = $(wildcard *.cpp)
OBJ            = $(CPPSRC:.cpp=.o)
LINKER_SCRIPT  = linker-script.x
MCU            = attiny841
# Size of the bootloader area. Must be a multiple of the erase size
BL_SIZE        = 2048
# Set the flash erase page size for the MCU here.
ERASE_SIZE     = 64

CXXFLAGS       =
CXXFLAGS      += -g3 -mmcu=$(MCU) -std=gnu++11
CXXFLAGS      += -Wall -Wextra
CXXFLAGS      += -Os -fpack-struct -fshort-enums
CXXFLAGS      += -flto -fno-fat-lto-objects
CXXFLAGS      += -DF_CPU=8000000UL
CXXFLAGS      += -DSPM_ERASESIZE=$(ERASE_SIZE)
CXXFLAGS      += -D$(BOARD)
CXXFLAGS      += -DPROTOCOL_VERSION=$(PROTOCOL_VERSION)

LDFLAGS        =
LDFLAGS       += -mmcu=$(MCU)

# Use a custom linker script
LDFLAGS       += -T $(LINKER_SCRIPT)
# Pass BL_SIZE to the script for positioning
LDFLAGS       += -Wl,--defsym=BL_SIZE=$(BL_SIZE)
# Pass ERASE_SIZE to the script to verify alignment
LDFLAGS       += -Wl,--defsym=ERASE_SIZE=$(ERASE_SIZE)


CC             = avr-gcc
OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump
SIZE           = avr-size

interface_v1.3:
	$(MAKE) all PRG=bootloader-iface-v1.3 BOARD=BOARD_IFACE_V1_3

all: hex fuses size

hex:  $(PRG).hex

fuses:
	@if $(OBJDUMP) -s -j .fuse 2> /dev/null $(PRG).elf > /dev/null; then \
		$(OBJDUMP) -s -j .fuse $(PRG).elf; \
		echo "        ^^     Low"; \
		echo "          ^^   High"; \
		echo "            ^^ Extended"; \
	fi

size:
	$(SIZE) --format=avr $(PRG).elf
clean:
	rm -rf $(OBJ) $(OBJ:.o=.d) *.elf *.hex *.lst *.map

$(PRG).elf: $(OBJ) $(LINKER_SCRIPT)
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -MMD -MP -c -o $@ $<

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

.PHONY: all lst hex clean fuses size

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.d)
