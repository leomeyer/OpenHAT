# Target file name (without extension).
TARGET = rpi_remoteswitch

# OPDI platform specifier
PLATFORM = linux

# Relative path to the openhat application directory.
# All other paths are calculated relative to this path, so if you create a plugin anywhere it should be enough
# just to adjust this path.
APP_PATH = ../../src

# Relative path to common directory (without trailing slash)
# This also becomes an additional include directory.
CPATH = $(APP_PATH)/../../../common

# Relative path to platform directory (without trailing slash)
# This also becomes an additional include directory.
PPATHBASE = $(APP_PATH)/../../../platforms
PPATH = $(PPATHBASE)/$(PLATFORM)

# List source files of the plugin here.
SRC = $(TARGET).cpp

# C++ wrapper
CPPPATH = $(APP_PATH)

# C++ wrapper files
SRC += $(CPPPATH)/OPDI_Ports.cpp

# additional source files (WiringPi)
SRC += $(APP_PATH)/../../../libraries/rpi/wiringPi/wiringPi.c
# $(APP_PATH)/../../../libraries/rpi/wiringPi/gb_spi.c $(APP_PATH)/../../../libraries/rpi/wiringPi/gb_pwm.c

# additional source files (raspberry-remote)
SRC += $(APP_PATH)/../../../libraries/rpi/raspberry-remote/RCSwitch.cpp

SRC += $(APP_PATH)/../../../platforms/linux/opdi_platformfuncs.c

# library include paths
LIBINCPATH = $(APP_PATH)/../../../libraries/rpi/wiringPi $(APP_PATH)/../../../libraries/rpi/raspberry-remote

# POCO include path
POCOINCPATH = $(APP_PATH)/../../../libraries/POCO/Util/include $(APP_PATH)/../../../libraries/POCO/Foundation/include $(APP_PATH)/../../../libraries/POCO/Net/include

# POCO library path
POCOLIBPATH = $(APP_PATH)/../../../libraries/POCO/lib/Linux/x86_64

# POCO libraries
POCOLIBS = -lPocoUtil -lPocoNet -lPocoFoundation 

# Additional libraries
LIBS = -lpthread -lwiringPi

# Additional library paths
LIBPATHS = $(APP_PATH)/../../../libraries/rpi/wiringPi

# The compiler to be used.
CC = g++

# List any extra directories to look for include files here.
# Each directory must be separated by a space.
EXTRAINCDIRS = $(CPATH) $(CPPPATH) $(PPATHBASE) $(PPATH) $(POCOINCPATH) $(APP_PATH) $(LIBINCPATH) .

# Place -I options here
CINCS =

# Defines
CDEFINES = -Dlinux

# Compiler flags.
CFLAGS = -Wall -fpermissive $(CDEFS) $(CINCS) $(patsubst %,-L%,$(LIBPATHS)) -L $(POCOLIBPATH) $(CDEFINES) -Wno-narrowing
CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS)) -O3 -std=c++11 -static-libstdc++

OBJECTS = $(SRC)

all: $(SRC) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -shared -fPIC -O3 -o $@.so $(POCOLIBS) $(LIBS)

.cpp.o:
	@echo
	@echo Compiling: $<
	$(CC) -c $(CFLAGS) $< -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(PPATH)/*.o
	rm -f $(CPATH)/*.o
	rm -f $(MPATH)/*.o
	rm $(TARGET)

