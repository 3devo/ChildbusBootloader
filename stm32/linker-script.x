/*

This linker script:
 - Absolutely positions the board info at the end of the bootloader
   flash area.

This linker script creatively uses some peculiarities in how ld treats
linker scripts. It is intended to be passed to the -T option on the
commandline. The following happens:
 - The -T option makes ld read this linker script
 - If it finds an "INSERT" statement, it will *also* read the default
   linker script (merging them into a single script, starting with this
   script, followed by the default script).
 - The INSERT statement affects all statement/sections *before* it, and
   inserts them into the right place later in the linker script.
 - Anything after the INSERT statement is treated as part of the actual
   linker script normally, which is what the attiny script uses to
   override some existing sections, but that is not needed here.

*/

SECTIONS
{
   BL_END = .;

   /* Put board_info at the end of flash, and omit the data from the elf
    * file (NOLOAD) since the contents must be flashed separately anyway
    * (and if the elf file already contains dummy board_info data, it
    * cannot be overwritten without another erase. */
   BOARD_INFO_START = ORIGIN(rom) + FLASH_APP_OFFSET - BOARD_INFO_SIZE;
   .text.board_info BOARD_INFO_START (NOLOAD) : {
      ASSERT(BL_END < BOARD_INFO_START, "Bootloader too big");
      *(.board_info)
   }
}

INSERT AFTER .text;
