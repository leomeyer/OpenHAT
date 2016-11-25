# Target file name (without extension).
TARGET = $(shell basename $(CURDIR))

# OPDI platform specifier
PLATFORM = linux

APP_PATH = .

OPDI_CORE_PATH = $(APP_PATH)/../../../opdi_core

# Relative path to common directory (without trailing slash)
# This also becomes an additional include directory.
CPATH = $(OPDI_CORE_PATH)/code/c/common

# Relative path to platform directory (without trailing slash)
# This also becomes an additional include directory.
PPATHBASE = $(OPDI_CORE_PATH)/code/c/platforms
PPATH = $(PPATHBASE)/$(PLATFORM)

# List C source files of the configuration here.
SRC = LinuxOpenHAT.cpp Configuration.cpp SunRiseSet.cpp TimerPort.cpp ExpressionPort.cpp ExecPort.cpp

# platform specific files
SRC += $(PPATH)/opdi_platformfuncs.c

# common files
SRC += $(CPATH)/opdi_message.c $(CPATH)/opdi_port.c $(CPATH)/opdi_protocol.c $(CPATH)/opdi_slave_protocol.c $(CPATH)/opdi_strings.c
SRC += $(CPATH)/opdi_aes.cpp $(CPATH)/opdi_rijndael.cpp

# master implementation
MPATH = $(CPATH)/master

# master files
# SRC += $(MPATH)/opdi_AbstractProtocol.cpp $(MPATH)/opdi_BasicDeviceCapabilities.cpp $(MPATH)/opdi_BasicProtocol.cpp $(MPATH)/opdi_DigitalPort.cpp $(MPATH)/opdi_IODevice.cpp $(MPATH)/opdi_main_io.cpp $(MPATH)/opdi_OPDIMessage.cpp $(MPATH)/opdi_MessageQueueDevice.cpp $(MPATH)/opdi_OPDIPort.cpp $(MPATH)/opdi_PortFactory.cpp $(MPATH)/opdi_ProtocolFactory.cpp $(MPATH)/opdi_StringTools.cpp $(MPATH)/opdi_TCPIPDevice.cpp

# C++ wrapper
CPPPATH = $(APP_PATH)

# C++ wrapper files
SRC += $(CPPPATH)/OPDI.cpp $(CPPPATH)/OPDI_Ports.cpp

# additional source files
SRC += ./AbstractOpenHAT.cpp ./Ports.cpp ./openhat_linux.cpp

# POCO include path
POCOINCPATH = $(OPDI_CORE_PATH)/code/c/libraries/POCO/Util/include $(OPDI_CORE_PATH)/code/c/libraries/POCO/Foundation/include $(OPDI_CORE_PATH)/code/c/libraries/POCO/Net/include

# POCO library path
POCOLIBPATH = $(OPDI_CORE_PATH)/code/c/libraries/POCO/lib/Linux/x86_64

# POCO libraries
POCOLIBS = -lPocoUtil -lPocoNet -lPocoFoundation -lPocoXML -lPocoJSON

# ExprTk expression library path
EXPRTK = ../../../libraries/exprtk

# libctb serial communication library
LIBCTB = $(OPDI_CORE_PATH)/code/c/libraries/libctb
LIBCTBINC = $(LIBCTB)/include
SRC += $(LIBCTB)/src/fifo.cpp $(LIBCTB)/src/getopt.cpp $(LIBCTB)/src/iobase.cpp $(LIBCTB)/src/kbhit.cpp $(LIBCTB)/src/linux/serport.cpp
SRC += $(LIBCTB)/src/linux/timer.cpp $(LIBCTB)/src/portscan.cpp $(LIBCTB)/src/serportx.cpp

# Additional libraries
LIBS = -lpthread -ldl -lrt

# The compiler to be used.
CC = g++

# List any extra directories to look for include files here.
# Each directory must be seperated by a space.
EXTRAINCDIRS = $(CPATH) $(CPPPATH) $(MPATH) $(PPATHBASE) $(PPATH) $(POCOINCPATH) $(CONIOINCPATH) $(EXPRTK) $(LIBCTBINC) .

# Place -I options here
CINCS =

# Defines
CDEFINES = -Dlinux -DPOCO_STATIC

# Compiler flags.
CFLAGS = -Wall -Wextra $(CDEFS) $(CINCS) -L $(POCOLIBPATH) $(CDEFINES) -Wno-unused-parameter
CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS)) -std=c++11 -static-libstdc++ -O2

# Debug flags
CFLAGS += -ggdb -ftrapv -fsanitize=undefined

OBJECTS = $(SRC)

all: $(SRC) plugins $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(POCOLIBS) $(LIBS)

.cpp.o:
	@echo
	@echo Compiling: $<
	$(CC) -c $(CFLAGS) $< -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

plugins:
	$(MAKE) -C ../plugins

clean:
	rm -f $(PPATH)/*.o
	rm -f $(CPATH)/*.o
	rm -f $(MPATH)/*.o
	rm $(TARGET)

