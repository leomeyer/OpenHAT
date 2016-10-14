Raspberry Pi Gertboard plugin for OpenHAT

This plugins supports most of the Gertboard I/O functions.
See openhat/testconfigs/gertboard.ini for a detailed description of configuration settings.

To use the port expansion via the on-board ATMEGA328P you have to program the microcontroller with the hex file in the
AtmegaPortExpander directory. You can do this via the on-board ISP connector or using the method described in the Gertboard manual.

Expansion port communication happens via serial port (/dev/ttyAMA0 on Raspbian). You should make sure that the serial port
is accessible and works as expected. On the Gertboard you should connect pin 14 and 15 to the adjacent MCRX and MCTX pins.

To test the expansion port communication you can send bytes in the lower ASCII range to the serial port (try A-Z). The controller responds
to each byte with a return byte. Use e. g. minicom (sudo minicom -p /dev/ttyAMA0 -b 19200).

Serial communication occurs in the OpenHAT main thread. If serial communication is slow, e. g. if the system load is high, and
necessarily serial communication must use the full system stack (it's doing I/O via file descriptors) you may experience timeouts.
Also, the doWork loop of ports is not served during I/O wait times. This may cause unexpected delays in event processing.

Generally though, expansion port communication should work well. In performance critical situations it's however best to avoid it.

