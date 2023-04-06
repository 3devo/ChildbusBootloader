/*

This linker script:
 - Just loads the default linker script for now.

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

INSERT AFTER .text;
