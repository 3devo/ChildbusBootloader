Childbus protocol
=================
This protocol allows communicating with the bootloader running on an
attiny or other small microcontroller, through I²C. It is intended to
support a composite embedded system, where a single mainboard controls a
number of child boards connected to a shared bus. This protocol then
facilitates the mainboard uploading an application to each child bus and
then executing that application.

This document describes protocol version 2.1 (0x0201).

History, compatibility and intended use
---------------------------------------
The initial version of this protocol was intended to be used over I²C
only to allow the bootloader to share the same bus as the SSD1306
display it was connected with. The initial bootloader implementation
used the low level I²C implementation and framing from the
"bootloader-attiny" by Erin Tomson from Modulo, but the upper layer of
the protocol (the available commands) were redesigned from scratch.

The intended usecase for this protocol and bootloader is a integrated
system with a single master controller, with multiple attached child
controllers:
 - At poweron, each child starts the bootloader. This bootloader listens
   on the bus for commands for the master. There is no timeout, so the
   bootloader will just wait if the master does not send any info.
 - The master detects all children, assignes each a unique address and
   uploads an application to each. This happens again on every boot
   (though the child takes care to prevent a flash erase and write cycle
   when the data is identical).
 - The master tells the child to start the application.

So this protocol is explicitly not intended to be used on standalone
boards, to upload software from a computer or so.

Initially, only the I²C bus was supported, but since that bus is prone
to noise problems when used for off-board connections, RS485 support was
later added. The upper layer of the protocol is identical between I²C
and RS485, but the lower level (framing) is different.

For RS485, the framing is copied from ModBus, which ensures that this
protocol can be used on a bus where ModBus is also used, without any
conflicts (provided that distinct addresses are used). That is, from the
point of view of other ModBus servers, all ChildBus messages look like
valid ModBus messages addressed to someone else. The intention is
explicitly not to allow using a complete ModBus client implementation to
communicate with a Childbus child.

Note that the physical layer is intended to be RS485, but any
(minimally) half-duplex byte stream that provides the necessary timing
can be used (i.e. TTL serial is fine, TCP is not).

Transactions and framing (I²C)
------------------------------
All transactions take the form of a write transfer (command plus
arguments), followed by a read transfer (status plus command result).

A write transfer consists of: A command byte, any number of argument
bytes and one CRC byte.

A read transfer consists of: A status byte, a length byte, any number of
result bytes and one CRC byte.

Visually, this looks like the following. A white background indicates
the line is controlled by the master, a gray background indicates the
line is controlled by the child.

<table>
<tr>
<td>Start</td><td>address+W</td>
<td>command</td>
<td>argument bytes...</td>
<td>CRC</td>
<td>Stop (optional)</td>
</tr>
<tr>
<td>Start</td><td>address+R</td>
<td style="background-color: #aaa">status</td>
<td style="background-color: #aaa">number of result bytes</td>
<td style="background-color: #aaa">result bytes ...</td>
<td style="background-color: #aaa">CRC</td>
<td>Stop</td>
</tr>
</table>

Acks and nacks are omitted from this table, but are present after each
byte. All bytes should be acked by the other party than the one that
sent the data. The last byte in a read transfer should always be acked
by the master, so the child knows to release the bus.

The maximum length of a transfer is variable, see the
`GET_MAX_PACKET_LENGTH` command for more details.

The number of result bytes in the transfer indicates the number of
result bytes, excluding the the status, the number itself and the CRC.

The master should either look at the length byte to know how much bytes
to read, or it should make sure to read enough bytes so all possibly
expected replies would fit. If the master is unable to look at the
length byte during the transfer and thus reads more bytes than the child
has available, the child should just return arbitrary values. The master
should ignore these by looking at the length byte after the transfer.
Alternatively, it could do a short read first, look at the length byte
and then do a longer read to get the full reply.

The CRC is calculated over all bytes, except the address byte.

Serial settings and timings (RS485)
-----------------------------------
Like with ModBus, the serial settings can be varied depending on the
needs of the network. The default settings are 19200 bps, one start bit,
8 data bits, even parity and one stop bit.

ModBus uses an interframe timeout (t3.5) which is nominally the length
of 3.5 bytes up to 19200 bps, but is specified as a fixed 1750μs for
higher baudrates.

In practice, these default settings (19200 bps and 1750μs t3.5) result
in excessive transfer times when uploading bigger flash contents (38
seconds for transferring 64k), so using a higher baudrate and/or lower
interframe timeout is recommended.

ModBus also specifies a maximum interbyte spacing (t1.5), considering
a message invalid if this maximum spacing is exceeded inside a frame.
To simplify implementations, a child does not need to check this
spacing, though a master should comply with it.

Transactions and framing (RS485)
--------------------------------
All transactions take the form of a request (master-to-child, command
plus arguments), followed by a reply (child-to-master, status plus
command result).

A request consists of: An address byte, command byte, any number of argument
bytes and two CRC bytes.

A write transfer consists of: An address byte, a status byte, a length
byte, any number of result bytes, and one CRC byte.

Visually, this looks like the following. A white background indicates
the line is controlled by the master, a gray background indicates the
line is controlled by the child.

<table>
<tr>
<td>address</td>
<td>command</td>
<td>argument bytes...</td>
<td>CRC LSB</td>
<td>CRC MSB</td>
</tr>
<tr>
<td style="background-color: #aaa">address</td>
<td style="background-color: #aaa">status</td>
<td style="background-color: #aaa">number of result bytes</td>
<td style="background-color: #aaa">result bytes ...</td>
<td style="background-color: #aaa">CRC LSB</td>
<td style="background-color: #aaa">CRC MSB</td>
</tr>
</table>

The maximum length of a request or reply is variable, see the
`GET_MAX_PACKET_LENGTH` command for more details.

The number of result bytes in the transfer indicates the number of
result bytes, excluding the address, the status, the number
itself and the CRC.

The CRC is calculated over all bytes, including the address byte (this
deviates from the I²C version).

The end of a request or reply is signaled by a period of silence, called
t3.5 by the ModBus specification. See below for recommendations on its
length.

Status codes
------------
The status byte can have these values:

| Value       | Meaning                           | Result bytes
|-------------|-----------------------------------|--------------
| 0x00        | `COMMAND_OK`                      | Command-specific
| 0x01        | `COMMAND_FAILED`                  | Command-specific
| 0x02        | `COMMAND_NOT_SUPPORTED`           | None
| 0x03        | `INVALID_TRANSFER`                | None
| 0x04        | `INVALID_CRC` (I²C only)          | None
| 0x05        | `INVALID_ARGUMENTS`               | None
| 0xff        | Unused                            |

When multiple reads happen in succession, each should return the same
data. This allows the master to re-try a read when it detects a CRC
error in the reply, without having to re-issue the command again.

Multi-byte values
-----------------
Any multi-byte values specified in this protocol should be transmitted
in network order (big endian), so with the most significant byte first.
As an exception to this, the CRC on RS485 messages is transmitted
little-endian, for compatibility with the Modbus protocol.

Clock stretching (I²C only)
---------------------------
The child is allowed to apply clock stretching at any time during a
transaction in order to process the command given or for other reasons.
This includes the entire write *and* read, to allow for example the
child to execute the command either directly after receiving it, after
seeing a stop condition or only during the read just before sending its
reply.

The total clock stretching that may be applied by the child during an
entire transaction is 80ms. Having a maximum stretch time allows the
master to detect a timeout and abort the transaction. The master is also
allowed to stretch the clock, no bounds are imposed on that. The child
should not attempt to detect a timeout itself, but instead rely on the
master. The child should instead monitor the bus for a start condition,
to always allow the master to start a new read or write.

Retries and CRC failures
------------------------
On I²C, the master can retry a failed read, for example when a CRC error
occurs on read. On subsequent reads, the child will return the exact
same reply.

On RS485 replies are initiated by the child, not the master,
and there is no mechanism for re-requesting a reply a second time (to
keep the child implementation simple and allow sharing a single buffer
between the incoming request and an outgoing reply). This means that on
CRC errors or reply timeouts, the reply must be considered lost and the
master should retry the command instead (all commands are specified in
such a way that this should always be possible).

On RS485, when a child detects a CRC error, it must *not* send an
`INVALID_CRC` reply, but instead drop and ignore the request. This
prevents a situation where a request is sent to child A, but is received
with a bit error in the address byte by child B. If child A does receive
the request correctly, then both childs might think the request is for
them and send conflicting replies. To prevent this, a reply should only
be sent when the CRC is correct. This means that bit errors in the
request will result in a master timeout.

Maximum response time (RS485 only)
----------------------------------
The child must start sending their response within 80ms. More
specifically, this is the maximum time between the end of the
inter-frame interval (t3.5) and the start bit of the first response
byte.

If no response is sent within that time (plus some margin), the master
should assume that the response was lost and will no longer be sent and
should act accordingly (i.e. retry or report failure). If a child
finalizes a reply only after this timeout, it should drop it rather than
send it too late, to prevent bus contention.

Note that this maximum response time was chosen to be long enough to
allow a full page erase and write on a STM32G0 MCU (using fast
programming), to prevent needing the additional (protocol and
implementation) complexity of polling for completion. If future
implementations need longer response times, the maximum response time
could be extended, or additional polling mechanisms could be added.

Multiple children
-----------------
When multiple children are connected to the same bus, a muster must have
some ways to distinguish them.

There are currently two ways to achieve this:

 1. By using the "Hardware type" field of the `SET_ADDRESS` command.
    This allows the master to reconfigure the address of a single child
    based on its hardware type before further querying it. A big
    limitation of this approach is that it only works when all children
    have unique hardware types, and it also requires "blindly"
    reassigning addresses for each hardware type that might be present
    before knowing that a device is actually present.

    This approach was implemented for the I²C version of this protocol,
    but never actually used in practice (since only one device was
    present on the bus anyway).

 2. By using "child select" pins. These are similar to the Chip Select
    pins used in the SPI interface, where the bootloader only responds
    to commands to the initial address range when its child select pin
    is asserted. Commands addressed to the address configured by
    `SET_ADDRESS` and general call commands are processed
    unconditionally.

    This requires an extra wire to be routed to each child, but
    application firmware typically already requires a status or irq pin
    which can be reused as a child select pin (which is used in the
    master-to-child direction when the bootloader runs and is used in
    the child-to-mastr direction when the application firmare runs). To
    prevent short-circuits when both sides accidentally drive the pin,
    adding a series resistor is recommended.

    This pin is intended to be active-low open-collector with a pullup
    on the master side (though push-pull can also be used, with a
    slightly higher chance of conflicts).

    The pin must be kept asserted or deasserted throughout the entire
    transaction, if the pin status is changed halfway through a
    transaction, it is undefined whether the child will respond or not.

    When its child select pin is not asserted, a child is allowed to
    respond with ACKs to an I²C write transaction (to simplify
    implementation), but it must not ACK the subsequent read
    transaction.

    To simplify wiring, children can also be connected in a tree
    topology, where any child may have downstream connectors to connect
    additional "nested" children to the bus. Each of these downstream
    connectors contains a distinct child select pin, each of which can
    be asserted individually using the `SET_CHILD_SELECT` command.

    It is expected that the application firmwares further support this
    topology by providing a `GET_STATUS_PINS` or similar command to
    query the value of each downstream status pin, and will assert their
    upstream status pin when any downstream status pin is asserted.

    This toplogy means that the master only needs a single child select
    pin for each directly-connected child, at the expense of some
    additional overhead accessing the child select or status pins.

    To enumerate the bus, the master starts by deasserting all child
    select pins and issues a general call reset (which causes all
    children to deassert their downstream child select pins). Then, it
    asserts one child select pin, discovers if any child is connected,
    reconfigures its address and deasserts the pin again. This process
    is repeated for all of its own child select pins, as well as any
    child select pins on the discovered children, allowing it to
    discover all connected children as well as how they are connected
    physically.

 Note that the first option (using hardware type) is always available in
 compliant bootloader implementations. However, there is no in-band
 mechanism for deciding or detecting if the child select mechansim is in
 use, so this must be coordinated based on the system design. For new
 designs, the child select mechanism is recommended because it gives the
 greatest flexibility.

 For 3devo, the I²C-based systems will remain configured to not use
 the child select mechanism (and probably be limited to a single device
 anyway). All future developments will use the RS485 transport and are
 intended to have child select enabled.

Initial addresses
-----------------
Instead of responding to a single address, the child will respond to a
range of different addresses. Even though this increases the chance of
one of these addresses colliding with other devices (i.e. other I²C or
other ModBus devices) in the system, there is only a small chance that
*all* of these addresses collide. The protocol includes a `SET_ADDRESS`
command that makes the child respond to just a single address, after
which all other addresses can be used as normal without conflicts.

The bootloader will initially respond to any address in the 8-15
range.

CRC (I²C)
---------
For I²C, the protocol uses CRC-8-CCITT, which should provide protection against all
1-bit and 3-bit errors, and good protection for 2-bit errors up to the maximum
message size. See the [publication by Philip Koopman][koopman] for CRC
comparisons. This CRC was mostly chosen because an efficient AVR implementation
is available and it seems to perform well enough.

Polynomial: x^8 + x^2 + x + 1 (0x83 / 0xE0) 
Starting value: 0xff 
No output XOR 

[koopman]: https://users.ece.cmu.edu/~koopman/crc/

CRC (RS485)
-----------
For RS485, the protocol uses CRC-16-IBM, as used by ModBus.

Polynomial: x^16 + x^15 + x^2 + 1 (0xC002 / 0xA001) 
Starting value: 0xffff 
No output XOR 
PyCRC command: `pycrc --model crc-16-modbus --check-hexstring 'DEADBEEF'` 

Version compatibility
---------------------
The child is assumed to have a small and fixed bootloader, which implements a
single version of this protocol. The master is assumed to be more complex and
easily upgraded, and will typically support all versions up to the current
version.

This means that a newer master with an older child should always work, since
the master has explicit support for older protocol versions. When an older
master (e.g. older firmware on the master) is running against a newer
child, things should just work if possible, but at least allow an error
message to be shown.

This protocol is initially designed to talk to a microcontroller on the
interface board. This microcontroller needs to take action to enable the
display before an error message can be shown to the user, so this protocol
contains some special provisions for this case (e.g. the `POWER_UP_DISPLAY`
command).

Additionally, the following rules apply:
 - All future version of this protocol, should implement at least the
   `GET_PROTOCOL_VERSION`, `SET_ADDRESS` and, if appropriate, the
   `POWER_UP_DISPLAY` commands, as specified in the 1.0 version of this
   protocol. All other commands can be changed in any way (including
   different framing, checksums, etc.), provided the protocol version is
   bumped appropriately.
 - Whenever a change is made that would cause a master implementing an
   older version of the protocol to stop working (commands no longer
   working or not having the intended effect), the major protocol
   version should be increased.
 - For all other changes, such as adding new commands or arguments, the
   minor protocol version is increased.

For the master, this means that it can always send the three commands
mentioned above, and any other commands must be sent after establishing
the protocol version in use (provided the master knows about it). If the
master knows about the major protocol version, but the minor protocol
version is higher that what it supports, the master can just assume the
minor version is the highest one it supports.

When processing replies, the master should be tolerant of unexpected
values and extra bits (ignore them if possible). If this behaviour
would break things (e.g. a new reply value that requires the master to
act differently), the major version should be bumped, otherwise only the
minor version would suffice. Note that adding extra bytes to a reply
always requires bumping the major version, since (on I²C) the master
dictates how much bytes are read.

Applications
------------
The bootloader is intended to allow uploading an application into flash
and to start it. The intention is that on *every* startup, the master
uploads an application (with the child taking care to prevent an
erase-write cycle if the same application is uploaded twice). For this
reason, the bootloader has no timeout value and does not start the
application unless explicitely instructed.

This means that the application that is run (after being uploaded by the
master) is strongly tied to the master application. This removes all
need for protocol compatibility between application versions, or between
the bootloader and the application. The only thing the application
*must* support is the general call reset command (see the generall call
section elsewhere in this document).

However, it is encouraged for the application to use the same framing
format (write/read pairs, checksum, timeout constraints, command and
status codes, etc.). Any commands that make sense (e.g.
`SET_ADDRESS`) can also be implemented, new commands should be
defined in the range reserved for application commands.

An application firmware is also encouraged to implement the
`GET_PROTOCOL_VERSION` command to allow identifying a successfully
started application firmware using a generic command, as well as
distinguishing a running bootloader from an application with a single
command if needed.

If the application firmiware implements `GET_PROTOCOL_VERSION`, it
should return a version of 0.0 (0x00, 0x00).

General call handling
---------------------
The bootloader and the application should support the general call
address (0x00), and in particular the reset and reset address commands.

On I²C, a general call consists of a write to the 0x00 I2c address,
followed by a command byte. No additional framing (e.g. no CRC) happens,
and no reply is read in response to the command. The command byte uses
the standard I²C meaning of 0x04 for "reset address" and 0x06 for reset.

On RS485, a general call consists of a full frame consisting of the 0x00
address, the commandcode and 2 CRC bytes. The command bytes are 0x44 for
"reset address" and 0x46 for "reset". These are arbitrary values chosen
from a "user-defined function codes" block in the Modbus specification
to minimize chance of conflicts with other Modbus servers that also
listen to the general call command.

The reset address command (0x04/0x44) should revert the effect of the
`SET_ADDRESS` command and make the bootloader respond to the default
addresses again. If the application also implements `SET_ADDRESS` or
something similar, it is recommended to also implement the reset address
command.

The reset command (0x06/0x46) should do a hardware reset of the child
(which should start the bootloader again and as a side effect reset the
address as well).

The master should always send a general call reset command at startup,
which serves to get all children into a known state, running the
bootloader. This prevents issues when the master resets, while the child
does not, resulting in the child running application code while the
master needs to talk to the bootloader.

Commands
========
The base protocol defines these commands:

| Value       | Meaning
|-------------|---------
| 0x00        | `GET_PROTOCOL_VERSION`
| 0x01        | `SET_ADDRESS`
| 0x02        | `POWER_UP_DISPLAY`
| 0x03        | `GET_HARDWARE_INFO`
| 0x04        | `GET_SERIAL_NUMBER`
| 0x05        | `START_APPLICATION`
| 0x06        | `WRITE_FLASH`
| 0x07        | `FINALIZE_FLASH`
| 0x08        | `READ_FLASH`
| 0x09        | `GET_HARDWARE_REVISION`
| 0x0a        | `GET_NUM_CHILDREN`
| 0x0b        | `SET_CHILD_SELECT`
| 0x0c        | `GET_MAX_PACKET_LENGTH`
| 0x80 - 0xfe | Reserved for application commands
| 0xff        | Reserved

`GET_PROTOCOL_VERSION` command
------------------------------
This command returns the protocol version implemented by the child split into a
major and minor part. For example, version 1.0 would be 0x01, 0x00, while
version 10.2 would be 0x0a, 0x02.

When this command is implemented by an application firmware, it should
always return a version of 0.0 (0x00, 0x00).

See the section on compatibility on how the master is expected to handle
this version number.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_PROTOCOL_VERSION` (0x00)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1     | Major version
| 1     | Minor version
| 1/2   | CRC

`SET_ADDRESS` command
-------------------------
This command allows changing the address, to prevent conflicts with
other devices.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `SET_ADDRESS` (0x01)
| 1     | Address to use
| 1     | Hardware type
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1/2   | CRC

On I²C, the address given is the 7-bit address, where the MSB is ignored
by the child and must be set to 0 by the master. On RS485, the address
is just the full 8-bit address.

The device type is a selector to select only devices of a particular
type. This can be used when multiple devices share the same bus and
respond to the same address, so they can still be disambiguated based on
their device type.

When a child receives this command containing a hardware type that does
not equal its own type (as returned by `GET_HARDWARE_INFO`), it should
completely ignore the command (and not respond to the subsequent read
request either).

A device type of 0x00 is a wildcard, all devices should process that.
Masters should only use this when they are certain that at most one
device will be respond, since there is no provision to handle conflicts.

On I²C, the change in address can happen either before or after the
response is read. The master should read a response from the old
address, and if no reply is received, it should read from the new
address.

On RS485, the response should include the old address from before the
switch.

`POWER_UP_DISPLAY` command
--------------------------
This command is intended specifically for the interface board, and
serves as a fallback in case the master cannot normally upload a
firmware and power up the display using that (because the bootloader
protocol is too new, or uploading repeatedly fails, etc.). When this
command is given, the bootloader executes the proper sequence for
powering up the on-board display. No initialization commands are sent to
the display, this should be done by the master.

By using clock stretching (for I²C) or delaying the reply (for RS485),
the child should ensure the transaction does not complete before the
display is powered on and ready.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `POWER_UP_DISPLAY` (0x02)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00) or `COMMAND_NOT_SUPPORTED` (0x02)
| 1     | Length
| 1     | Controller type
| 1/2   | CRC

Modules that do not have an on-board display should just return
`COMMAND_NOT_SUPPORTED`.

The result includes single controller type byte, which indicates the
type of display controller attached. Currently only one type is defined:
0x01 to indicate an SSD1306 or compatible controller.

This command is intended as a last-resort option, since the
powerup-sequence is hardcoded and might not be ideal.

`GET_HARDWARE_INFO` command
---------------------------
This command requests information about the hardware the bootloader runs
on.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_HARDWARE_INFO` (0x03)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1     | Hardware type
| 1     | Compatible hardware revision
| 1     | Bootloader version
| 2     | Available flash size
| 1/2   | CRC

The following hardware types are defined:

| Type | Meaning
|------|----------------------
| 0x00 | Reserved for wildcard in `GET_I2C_ADDRESS` command
| 0x01 | Interface board
| 0x02 | Granulate processor hopper board

The compatible hardware revision field indicates the oldest revision
that this board is compatible with and may not reflect the actual board
revision. A board is compatible with another revision, if anything (i.e.
firmware) that is intended for the older version, will also work for the
newer board. In practice, this means a newer version only has minor
hardware optimizations, small improvements or additions that can be
ignored by older firmware. To obtain the actual hardware version, use
the `GET_HARDWARE_REVISION` command.

The hardware revision is split into a major and minor version. The upper
4 bits indicate the major version, while the lower 4 bits indicate the
minor version. For example, 0x15 means revision 1.5, while 0x2f means
revision 1.15.

The only exception is that interface board (type 0x01) version 1.3
should use a compatible hardware version value of 0x01, if compatibility
with protocol version 1.0 is intended.

The bootloader version value is informative (and should be considered
board-specific) and its value is not defined by this specification.

The available flash size indicates the number of bytes of flash that are
available to write to.

The max message size indicates the largest I²C message that can be sent
or received (including the checksum, excluding the address byte).

`GET_HARDWARE_REVISION` command
---------------------------
This command requests the actual hardware version of the board.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_HARDWARE_REVISION` (0x09)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1     | Hardware revision
| 1/2   | CRC

The returned hardware revision indicates the actual board version and
should be changed on every hardware change, even minor changes. Together
with the compatible hardware revision returned by the
`GET_HARDWARE_INFO` command, the master can determine whether it
supports this hardware version.

The hardware revision is split into a major and minor version. The upper
4 bits indicate the major version, while the lower 4 bits indicate the
minor version. For example, 0x13 means revision 1.3, while 0x2f means
revision 1.15.

This command was added in protocol version 1.1.

`GET_SERIAL_NUMBER` command
---------------------------
This command requests the serial number of the board, if available. If
no serial number is present, this command can return
`COMMAND_NOT_SUPPORTED`.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_SERIAL_NUMBER` (0x04)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| n     | Serial number
| 1/2   | CRC

The length and format of the serial number is device-dependent.

`START_APPLICATION` comand
--------------------------
This command tells the bootloader to start the application. To simplify
child implementation, this command returns no reply, so the application
can be started immediately after receiving the command, instead of
having to wait for a reply to be read first.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `START_APPLICATION` (0x05)
| 1/2   | CRC

`WRITE_FLASH` command
---------------------
This command allows writing an application to flash.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `WRITE_FLASH` (0x06)
| 2     | Address
| 0+    | Data
| 1/2   | CRC

The address specified indicates the (byte) address of the first byte to
write. Note that this address is relative to the writable flash area,
which might or might not be at the start of the device's flash (e.g. the
bootloader might be at the start of flash and an address 0 might refer
to the first available address after the bootloader). There is currently
no mechanism to query for such a "flash offset". When relevant (e.g. for
selecting the right image to flash), the master is expected to have
out-of-band knowledge (e.g. based on the device type, hardware version,
bootloader version, etc.) to determine the offset.

To simplify the child implementation, this command should only be used
to write consecutive bytes to flash, starting at address 0. In
particular the address must either be one past the last byte written, or
it must be zero to start, or start over. If this constraint is violated,
the child should return `INVALID_ARGUMENTS` and otherwise ignore the
packet (so the next write should be accepted if it is consecutive with
the previous valid write). This allows the master to retry a write when
it receives in invalid checksum on a reply. It should then ignore the
`INVALID_ARGUMENTS` error on the retry.

Writing to flash in this way guarantees that the sent bytes are
(eventually) written, but the rest of the flash contents becomes
undefined (e.g. parts of it will likely be erased).

Due to flash page sizes, the data sent might not be written immediately,
but is typically buffered until a full flash page can be written (and is
then automatically written). At the end of flashing, the
`FINALIZE_FLASH` command must be given to commit any remaining bytes to
flash.

When the bytes sent are identical to the current content of the flash,
the child should prevent an erase-write cycle. This allows the master
to send write commands on every startup, without danger of wearing out
the flash contents.

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `INVALID_ARGUMENTS` (0x05)
| 1     | Length
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_FAILED` (0x01)
| 1     | Length
| 1     | Reason
| 1/2   | CRC

When flashing fails for any reason, an additional reason byte is
returned. The meaning of this byte is purely informative and not defined
by this protocol, its meaning should be looked up in the bootloader.

`FINALIZE_FLASH` command
------------------------
This command commits all unwritten bytes (as sent by `WRITE_FLASH`) to
the the flash memory. After this command, no further `WRITE_FLASH`
commands should be sent, except with a zero address to start over.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `FINALIZE_FLASH` (0x07)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1     | Erasecount
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_FAILED` (0x01)
| 1     | Length
| 1     | Reason
| 1/2   | CRC

The erasecount is the number of pages erased since the last reset, or
the last succesful `FINALIZE_FLASH` command. This is returned to
facilitate verification of the "erase only when needed" mechanism.

When flashing fails for any reason, an additional reason byte is
returned. The meaning of this byte is purely informative and not defined
by this protocol, its meaning should be looked up in the bootloader.

`READ_FLASH` command
------------------------
This command reads the specified number of bytes from the specified
address and returns them. This reads the current state of flash, so when
this is given between `WRITE_FLASH` and `FINALIZE_FLASH`, it might
return an inconsistent state.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `READ_FLASH` (0x07)
| 2     | Address
| 1     | Length
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 0+    | Data
| 1/2   | CRC

`GET_NUM_CHILDREN` command (optional)
-------------------------------------
This command returns the number of downstream child connectors and/or
child select pins this child has.

This command is optional, if it is not implemented,
`COMMAND_NOT_SUPPORTED` should be returned and the master should behave
as if a value of 0 was returned.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_NUM_CHILDREN` (0x0a)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1     | Number of children
| 1/2   | CRC

This command was added in protocol version 2.1.

`SET_CHILD_SELECT` command (optional)
-------------------------------------
This command sets the status of a downstream child select pin.

This command is optional, but if `GET_NUM_CHILDREN` is implemented,
`SET_CHILD_SELECT` must also be implemented.

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `SET_CHILD_SELECT` (0x0b)
| 1     | Index
| 1     | State
| 1/2   | CRC

The specified index is the index of the pin to control, which must be
greater than or equal to 0, and less than the number returned by
`GET_NUM_CHILDREN`.

The specified state is the intended state of the pin: 0 to deassert the
pin (make it Hi-Z), 1 to assert the pin (make it LOW).

If arguments outside these ranges are specified, `INVALID_ARGUMENTS`
will be returned.

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `INVALID_ARGUMENTS` (0x05)
| 1     | Length
| 1/2   | CRC

This command was added in protocol version 2.1.

`GET_MAX_PACKET_LENGTH` command (optional)
------------------------------------------
This command returns the maximum length of a request or reply packet
that the bootloader is prepared to accept or return.

What is included in the packet length depends on the protocol used:
 - For I²C, this is all data, including the CRC but excluding the
   address which is considered part of the underlying I²C protocol.
 - For RS485, this includes all data *including* the address, up to and
   including the CRC.

This command is optional, if it is not implemented,
`COMMAND_NOT_SUPPORTED` should be returned and the master should behave
as if a value of 32 was returned.

If this command is implemented, the value returned must be at least 32
(in other words, a master may assume that 32-byte packets are ok,
even without using this command).

| Bytes | Command field
|-------|-------------------------------
| 1     | Cmd: `GET_MAX_PACKET_LENGTH` (0x0c)
| 1/2   | CRC

| Bytes | Reply format
|-------|-------------------------------
| 1     | Status: `COMMAND_OK` (0x00)
| 1     | Length
| 2     | Max packet length
| 1/2   | CRC

This command was added in protocol version 2.1.

Changelog
=========
 - Version 1.0
   - Initial version.
 - Version 1.1
   - Redefined the hardware revision field to be the compatible
     hardware revision field instead and generalize its meaning to a
     major/minor format.
   - Added `GET_HARDWARE_REVISION` command.
 - Version 2.0
   - Extend maximum clock stretch time to 80ms. This needs a major
     version bump, since older masters might timeout at the previous
     value of 35ms.
 - Version 2.1
   - Add support for RS485.
   - Add support for child select pins (added `GET_NUM_CHILDREN` and
     `SET_CHILD_SELECT` commands and support responding only when child
     select pin is asserted).
   - Renamed to Childbus protocol.
   - Add `GET_MAX_PACKET_LENGTH` command.
   - Add hopper board hardware type.


License
=======
Permission is hereby granted, free of charge, to anyone
obtaining a copy of this document, to do whatever they want with them without
any restriction, including, but not limited to, copying, modification and
redistribution.

NO WARRANTY OF ANY KIND IS PROVIDED.
