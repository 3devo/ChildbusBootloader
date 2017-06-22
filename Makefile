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

PRG            = bootloader-attiny
CPPSRC         = $(wildcard *.cpp)
OBJ            = $(CPPSRC:.cpp=.o)
MCU            = attiny841

CXXFLAGS       =
CXXFLAGS      += -g3 -mmcu=$(MCU) -std=gnu++11
CXXFLAGS      += -Wall -Wextra
CXXFLAGS      += -Os -fpack-struct -fshort-enums
CXXFLAGS      += -flto -fno-fat-lto-objects
CXXFLAGS      += -DF_CPU=8000000UL

LDFLAGS        =
LDFLAGS       += -mmcu=$(MCU)
LDFLAGS       += -T bootloader-avr25.x

CC             = avr-gcc
OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: hex

lst:  $(PRG).lst

hex:  $(PRG).hex

clean:
	rm -rf $(OBJ) $(OBJ:.o=.d) $(PRG).elf $(PRG).hex $(PRG).lst $(PRG).map

$(PRG).elf: $(OBJ)
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -MMD -MP -c -o $@ $<

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

.PHONY: all lst hex clean

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.d)
