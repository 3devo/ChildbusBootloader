# Bootloader test suite
This Arduino sketch runs various tests on the bootloader. It is intended
to run on a 3devo devoboard, atmega2560 or other avr connected to the an
attiny (e.g. interfaceboard) running the bootloader through I²C, or
a GP20 mainboard connected to a GP20 hopperboard.

It depends on two libraries:
 - https://github.com/mmurdoch/arduinounit (with
   [PR #85 applied](https://github.com/mmurdoch/arduinounit/pull/85))
 - https://github.com/stevemarple/SoftWire (tested with 2.0.0 and 2.0.9)

To run the tests, build the childbus bootloader using the Makefile and
upload it to the child board as normal (see instructions in the parent
directory). Be sure to also upload board info contents. Example contents
to use are in the `board_info` subdirectory.

To test with different board info contents:
 - Make changes as needed to the .txt files in the `board_info`
   directory.
 - Run `make` in that directory. This calculates the correct checksum
   for you, that you can then manually put into the txt file (though the
   test sketch does not currently verify the checksum, keeping it
   correct is helpful to keep the board info useful for manual testing
   with a real master too). Do not forget to reverse the bytes for
   little-endian encodig.
 - Run `make` in that directory again. This generates a .hex file that you can
   upload to the child board, and a .h file that is included by the
   test suite (so after making changes, you need to re-upload the board
   info to the child board, and rebuild and reupload the test sketch).
 - If you make changes to the board versions or component variations,
   make sure to make these same changes in `Constants.h` so the test
   sketch knows what values to expect.

## Configuring the test sketch
When running tests you should review `Constants.h` to set the
to-be-expected values for the test sketch. In particular, select the
right test subject (attiny/stm32) and bus (i2c/rs485) to use for the
test.

Additionally, the default values for `struct Cfg` in
`BootloaderTest.ino` also influence the tests being performed (this is
where you can control debug printing and limit or expand flash writing
to save time and/or flash wear cycles). Note that some of the config
values in the struct are automatically modified during subsequent test
runs (so only determine the first test run value) and some are used for
all test runs.

## Child select pins
The protocol can be used with a child select pin for sharing a single
bus among multiple child devices. In practice, this is only used for the
gp20 hopperboards over rs485, while the I²C-based interfaceboards do not
have this enabled (for compatibility with earlier master
implementations). Be sure to set the `USE_CHILD_SELECT` macro in
`Constants.h` accordingly.

The gp20 hopperboard also supports a downstream childbus connector with
downstream childselect pin. To test that this pin is properly set by the
child, it should be connected (using a single wire) to a pin on the
board running the test sketch. See `DOWNSTREAM_CS_CHECK_PINS` in
`Constants.h` for the pin to use.

## Multiple test runs

The test sketch runs the same tests multiple times, but with different
settings (i.e. different slave addresses, with or without repeated
start, etc.). It first runs a number of them with fixed settings, then
waits for a newline on serial and after that runs randomized tests
indefinitely.

Usually, just the fixed tests (or even just one fixed test) is ok to
verify functionality, but for extra thorough testing you can also leave
the random tests running for a while.
