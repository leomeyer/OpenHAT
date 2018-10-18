# openhatd makefile
# Creates a release file that includes plugins.
# Pass DEBUG=1 to enable a debug build.

# Program file name
BASENAME = openhatd

DEBUG ?= 0

# Architecture, used for creating the release file
ARCH ?= x86_64

# OPDI platform specifier
PLATFORM ?= linux

# Target file name (without extension). For intermediate binary and tar file
TARGET = $(BASENAME)-$(PLATFORM)-$(ARCH)

# version from file
VERSIONRAW = $(shell cat VERSION)
VERSION = $(subst ",,$(VERSIONRAW))

# current timestamp
TIMESTAMP = $(shell date)

# build identification; if not specified, it is "current"
BUILD ?= current

MAKEFILE ?= makefile

APP_PATH = .

OPDI_CORE_PATH = $(APP_PATH)/../../opdi_core

LIBRARY_PATH = $(APP_PATH)/../../libraries/

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
POCOLIBPATH ?= $(OPDI_CORE_PATH)/code/c/libraries/POCO/lib/Linux/x86_64

# POCO libraries
# default: dynamic linking
ifeq ($(DEBUG),1)
	POCOLIBS ?= -lPocoUtild -lPocoNetd -lPocoFoundationd -lPocoXMLd -lPocoJSONd
else
	POCOLIBS ?= -lPocoUtil -lPocoNet -lPocoFoundation -lPocoXML -lPocoJSON
endif

# ExprTk expression library path
EXPRTK = $(LIBRARY_PATH)/exprtk

# libctb serial communication library
LIBCTB = $(OPDI_CORE_PATH)/code/c/libraries/libctb
LIBCTBINC = $(LIBCTB)/include
SRC += $(LIBCTB)/src/fifo.cpp $(LIBCTB)/src/getopt.cpp $(LIBCTB)/src/iobase.cpp $(LIBCTB)/src/kbhit.cpp $(LIBCTB)/src/linux/serport.cpp
SRC += $(LIBCTB)/src/linux/timer.cpp $(LIBCTB)/src/portscan.cpp $(LIBCTB)/src/serportx.cpp

# Additional libraries
LIBS += -lpthread -ldl -lrt

# The compiler to be used.
CC ?= g++

# List any extra directories to look for include files here.
# Each directory must be separated by a space.
EXTRAINCDIRS = $(CPATH) $(CPPPATH) $(MPATH) $(PPATHBASE) $(PPATH) $(POCOINCPATH) $(CONIOINCPATH) $(EXPRTK) $(LIBCTBINC) .

# Place -I options here
CINCS +=

# Defines
CDEFINES += -Dlinux

# Compiler flags.
CFLAGS += -Wall -Wextra $(CDEFS) $(CINCS) -L $(POCOLIBPATH) $(CDEFINES) -Wno-unused-parameter
CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS)) -std=c++11 -O2

# Linker flags
# Make resulting binaries look for shared objects in the current directory
LDFLAGS += -Wl,-rpath,'$$ORIGIN'

# POCO library version
POCO_LIBVERSION = $(shell cat $(POCOLIBPATH)/../../../libversion)

# Debug flags
ifeq ($(DEBUG),1)
	CFLAGS += -ggdb -ftrapv -fsanitize=undefined
else
    CFLAGS += -DNDEBUG
endif

OBJECTS = $(SRC)

TARFOLDER = $(TARGET)-$(VERSION)-$(BUILD)

all: clean plugins $(TARGET)

target: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo $(CC)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(POCOLIBS) $(LIBS) $(LDFLAGS)

.cpp.o:
	@echo
	@echo Compiling: $<
	$(CC) -c $(CFLAGS) $< -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

plugins:
	$(MAKE) -C ../plugins -f $(MAKEFILE)

docs:
ifeq (,$(wildcard ./openhatd-docs-$(VERSION).tar.gz))
	@echo Documentation already exists, skipping build.
else
	mv -f ../Doxyfile ../Doxyfile.orig
	sed "s/__VERSION__/$(VERSION)/g;s/__TIMESTAMP__/$(TIMESTAMP)/g" < ../Doxyfile.orig > ../Doxyfile
	mv -f ../mkdocs.yml ../mkdocs.yml.orig
	sed "s/__VERSION__/$(VERSION)/g;s/__TIMESTAMP__/$(TIMESTAMP)/g" < ../mkdocs.yml.orig > ../mkdocs.yml
	cd .. && doxygen Doxyfile && mkdocs build
	rm -f ../Doxyfile
	mv -f ../Doxyfile.orig ../Doxyfile
	rm -f ../mkdocs.yml
	mv -f ../mkdocs.yml.orig ../mkdocs.yml
	tar czf openhatd-docs-$(VERSION).tar.gz ../openhatd-docs-$(VERSION)
	md5sum openhatd-docs-$(VERSION).tar.gz > openhatd-docs-$(VERSION).tar.gz.md5
endif

release: docs all
	@echo Preparing tar folder...
	mkdir -p $(TARFOLDER)
	mkdir -p $(TARFOLDER)/bin
	@echo Copying main binary...
	cp $(TARGET) $(TARFOLDER)/bin/$(BASENAME)
ifeq ($(INCLUDE_POCO_SO),1)
	@echo Copying POCO libraries...
ifeq ($(DEBUG),1)
	cp $(POCOLIBPATH)/*d.so.$(POCO_LIBVERSION) $(TARFOLDER)/bin
else
	cp $(POCOLIBPATH)/*.so.$(POCO_LIBVERSION) $(TARFOLDER)/bin
	find $(TARFOLDER)/bin/ -type f -name '*d.so.*' -exec rm {} \;		
endif
endif
	cp hello-world.ini $(TARFOLDER)/bin/
	@echo Preparing plugin folder...
	mkdir -p $(TARFOLDER)/plugins
# TODO let plugin makefiles handle the packaging (?)
	find ../plugins/ -name '*.so' -exec cp --parents {} $(TARFOLDER)/plugins \;
	cp -r ../plugins/WebServerPlugin/webdocroot $(TARFOLDER)/plugins/WebServerPlugin
	@echo Copying testconfigs...
	cp -r ../testconfigs $(TARFOLDER)
	@echo Preparing documentation folder...
	mkdir -p $(TARFOLDER)/doc
	cp -r ../openhatd-docs-$(VERSION)/* $(TARFOLDER)/doc/
	find $(TARFOLDER)/doc/ -type f -name '*.vsdx' -exec rm {} \;
	@echo Making tar...
	tar czf $(TARFOLDER).tar.gz $(TARFOLDER)
	rm -rf $(TARFOLDER)
	@echo Calculating checksum...
	md5sum $(TARFOLDER).tar.gz > $(TARFOLDER).tar.gz.md5
	@echo Done.

tests:
	./$(TARGET) -c hello-world.ini -t -q
	./$(TARGET) -c ../testconfigs/dev.ini -t -q
	./$(TARGET) -c ../testconfigs/linux_test.ini -t -q
	./$(TARGET) -c ../testconfigs/weather_test.ini -t -q
	./$(TARGET) -c ../testconfigs/window_test.ini -t -q
	./$(TARGET) -c ../testconfigs/testconfig.ini -t -q

clean:
	find ../plugins/ -name '*.so' -exec rm {} \;
	rm -f $(PPATH)/*.o
	rm -f $(CPATH)/*.o
	rm -f $(MPATH)/*.o
	rm -f $(TARGET)
	rm -rf $(TARFOLDER)
	rm -f $(TARFOLDER).tar.gz*
	rm -f openhatd-docs-*
