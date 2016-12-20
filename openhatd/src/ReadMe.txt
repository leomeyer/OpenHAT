

Building on Linux

Building on Linux requires a compiler supporting C++11. GCC will support C++11 from version 4.8 onwards.


Building on Raspberry Pi

Set /usr/bin/g++ to link to a gcc version >= 4.8!
You might need to install a more recent version of gcc first.
Make sure that the POCO libraries are compiled.
Building on the RPi can take a very long time, though. It is usually better to use a cross-compiler.

To build OpenHAT for RPi using a cross-compiler, install your cross-compiler toolchain of choice.
For example: https://github.com/raspberrypi/tools
To build, use the following command:
> make -f makefile-rpicc
