Version 3.1
===========
- Support interfaceboard v1.5 and v1.6.
- Support hopperboard v2.0 and 2.1.

Version 3
=========
 - Support protocol version 2.1.
 - Support STM32G0.
 - Support ATtiny441 (untested).
 - Support RS485 (STM32G0 only).
 - Support erasing/flashing pages more than 256 bytes long.
 - Ignore general call messages that are longer than expected.
 - Support an upstream child select pin.
 - Support downstream childbus connections and the `GET_NUM_CHILDREN`
   and `SET_CHILD_SELECT` commands.
 - Support `GET_MAX_PACKET_LENGTH` and increase max packet length to 255
   on the hopperboard.
 - Support `GET_EXTRA_INFO` command and return the attached display
   hardware type on the interfaceboard.
 - Size optimizations for ATtiny (noreturn on startApplication and
   remove ISR).
 - Various refactoring and restructuring of the code to support Rs485
   and allow reusing parts of the code elsewhere.
 - Support hopperboard v1.0.

Version 2.1
===========
 - Support interfaceboard v1.5.

Version 2
=========
 - Support protocol version 1.1.
 - Add `GET_HARDWARE_VERSION` command.
 - Put version info in a fixed position in flash.
 - Support interfaceboard v1.4.

Version 1
=========
 - Initial release.
 - Supports ATtiny841.
 - Supports interfaceboard v1.3.

