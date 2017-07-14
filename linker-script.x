/*
 This linker script creatively uses some peculiarities in how ld
 treats linker scripts. It is intended to be passed to the -T option
 on the commandline. The following happens:
 - The -T option makes ld read this linker script
 - If it finds an "INSERT" statement, it will *also* read the default
   linker script.
 - The INSERT statement affects all statement/sections *before* it, and
   inserts them into the right place later in the linker script. In this
   case, it has no effect (but it is needed to let the default script be
   loaded).
 - When an output section occurs twice (such as here and an in the
   default linker script), the entries are merged into a single section.
   Most options (address, memory region, etc.) are overwritten by the
   second section spec (which is from the default script in this case,
   which is what we want). This merging ensures that the addresses are
   properly assigned within the section.

Effectively, this means the content of the .text section defined below
is inserted at the start of the .text section in the default linker
script.

An obvious way to have written this script was to put the `INSERT BEFORE
.text` statement at the end, which you might expect to achieve the same
effect. However, since the insert really works on the list of output
sections (not the list of statements in the linker script), both .text
output section statements will be merged into one section, this would
mean .text needs to be inserted before itself, which fails with ".text
not found for insert".

*/


/* This just serves to load the default linker script, it does not
 * actually insert anything. The section can be anything (though it must
 * exist). */
INSERT BEFORE .text;

SECTIONS
{
   .text : {
      /* Put the interrupt vector table at the start as normal. This is
       * to:
       *  - Properly set the reset vector to jump to the bootloader
       *  - Prevent the other (unused) interrupt vectors from eating up
       *    bootloader flash space. These will be overwritten when an
       *    application is flashed.
       * Putting this section here prevents it from being placed again
       * by the default linker script.
       */
      *(.vectors)
      KEEP(*(.vectors))

      /* Put the bootloader at the end of flash. This requires a
       * fixed size (BL_SIZE is passed on the linker commandline), since
       * we won't know the size until the placement is complete. */
      BL_START = ORIGIN(text) + LENGTH(text) - BL_SIZE;

      /* Put the boot_trampoline section just before the bootloader.
       * This is a single instruction function that jumps to the
       * application and is updated whenever a new application is
       * flashed. */
      . = BL_START - 2;
      *(.boot_trampoline)

      /* Verify that the boot_trampoline is the expected size */
      ASSERT(. == BL_START, "boot_trampoline section should be a single 2-byte instruction");

      /* Verify that the bootloader is aligned to a erase size boundary,
       * so changing boot_trampoline does not need to erase any part of
       * the bootloader. */
      ASSERT(BL_START % ERASE_SIZE == 0, "Bootloader should be aligned to erase size (change BL_SIZE)");
   }
}
