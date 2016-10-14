# Target file name (without extension).
TARGET = WeatherPlugin

# OPDI platform specifier
PLATFORM = linux

# Relative path to the openhat application directory.
# All other paths are calculated relative to this path, so if you create a plugin anywhere it should be enough
# just to adjust this path.
APP_PATH = ../../../openhat

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
CPPPATH = $(CPATH)/cppwrapper

# C++ wrapper files
SRC += $(CPPPATH)/OPDI_Ports.cpp

# additional source files
# SRC += 
SRC += $(APP_PATH)/../../../platforms/linux/opdi_platformfuncs.c

# conio include path
CONIOINCPATH = $(APP_PATH)/../../../libraries/conio

# POCO include path
POCOINCPATH = $(APP_PATH)/../../../libraries/POCO/Util/include $(APP_PATH)/../../../libraries/POCO/Foundation/include $(APP_PATH)/../../../libraries/POCO/Net/include

# POCO library path
POCOLIBPATH = $(APP_PATH)/../../../libraries/POCO/lib/Linux/x86_64

# POCO libraries
POCOLIBS = -lPocoUtil -lPocoNet -lPocoFoundation 

# Additional libraries
LIBS = -lpthread 

# The compiler to be used.
CC = g++

# List any extra directories to look for include files here.
# Each directory must be separated by a space.
EXTRAINCDIRS = $(CPATH) $(CPPPATH) $(PPATHBASE) $(PPATH) $(POCOINCPATH) $(CONIOINCPATH) $(APP_PATH) $(GERTBOARDINCPATH) .

# Place -I options here
CINCS =

# Defines
CDEFINES = -Dlinux

# Compiler flags.
CFLAGS = -Wall $(CDEFS) $(CINCS) -L $(POCOLIBPATH) $(CDEFINES)
CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS)) -ggdb -O2 -std=c++11 -static-libstdc++

OBJECTS = $(SRC)

all: $(SRC) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -shared -fPIC -o $@.so $(POCOLIBS) $(LIBS)

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

