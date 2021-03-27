#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include openhatd-Makefile.mk

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/b93babfe/opdi_AbstractProtocol.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_BasicDeviceCapabilities.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_BasicProtocol.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_DigitalPort.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_IODevice.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_MessageQueueDevice.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_OPDIMessage.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_OPDIPort.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_PortFactory.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_ProtocolFactory.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_SelectPort.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_StringTools.o \
	${OBJECTDIR}/_ext/b93babfe/opdi_main_io.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_aes.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_message.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_port.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_protocol.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_rijndael.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_slave_protocol.o \
	${OBJECTDIR}/_ext/e2dc17f3/opdi_strings.o \
	${OBJECTDIR}/_ext/a50b0fff/fifo.o \
	${OBJECTDIR}/_ext/a50b0fff/getopt.o \
	${OBJECTDIR}/_ext/a50b0fff/iobase.o \
	${OBJECTDIR}/_ext/a50b0fff/kbhit.o \
	${OBJECTDIR}/_ext/f5efeae4/serport.o \
	${OBJECTDIR}/_ext/f5efeae4/timer.o \
	${OBJECTDIR}/_ext/a50b0fff/portscan.o \
	${OBJECTDIR}/_ext/a50b0fff/serportx.o \
	${OBJECTDIR}/_ext/1522cf7d/opdi_platformfuncs.o \
	${OBJECTDIR}/src/AbstractOpenHAT.o \
	${OBJECTDIR}/src/Configuration.o \
	${OBJECTDIR}/src/ExecPort.o \
	${OBJECTDIR}/src/ExpressionPort.o \
	${OBJECTDIR}/src/LinuxOpenHAT.o \
	${OBJECTDIR}/src/OPDI.o \
	${OBJECTDIR}/src/OPDI_Ports.o \
	${OBJECTDIR}/src/Ports.o \
	${OBJECTDIR}/src/SunRiseSet.o \
	${OBJECTDIR}/src/TimerPort.o \
	${OBJECTDIR}/src/openhat_linux.o


# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L../opdi_core/code/c/libraries/POCO/lib/Linux/x86_64

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/openhatd

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/openhatd: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/openhatd ${OBJECTFILES} ${LDLIBSOPTIONS} -lPocoUtild -lPocoNetd -lPocoFoundationd -lPocoXMLd -lPocoJSONd -lpthread -ldl -lrt

${OBJECTDIR}/_ext/b93babfe/opdi_AbstractProtocol.o: ../opdi_core/code/c/common/master/opdi_AbstractProtocol.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_AbstractProtocol.o ../opdi_core/code/c/common/master/opdi_AbstractProtocol.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_BasicDeviceCapabilities.o: ../opdi_core/code/c/common/master/opdi_BasicDeviceCapabilities.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_BasicDeviceCapabilities.o ../opdi_core/code/c/common/master/opdi_BasicDeviceCapabilities.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_BasicProtocol.o: ../opdi_core/code/c/common/master/opdi_BasicProtocol.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_BasicProtocol.o ../opdi_core/code/c/common/master/opdi_BasicProtocol.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_DigitalPort.o: ../opdi_core/code/c/common/master/opdi_DigitalPort.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_DigitalPort.o ../opdi_core/code/c/common/master/opdi_DigitalPort.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_IODevice.o: ../opdi_core/code/c/common/master/opdi_IODevice.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_IODevice.o ../opdi_core/code/c/common/master/opdi_IODevice.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_MessageQueueDevice.o: ../opdi_core/code/c/common/master/opdi_MessageQueueDevice.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_MessageQueueDevice.o ../opdi_core/code/c/common/master/opdi_MessageQueueDevice.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_OPDIMessage.o: ../opdi_core/code/c/common/master/opdi_OPDIMessage.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_OPDIMessage.o ../opdi_core/code/c/common/master/opdi_OPDIMessage.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_OPDIPort.o: ../opdi_core/code/c/common/master/opdi_OPDIPort.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_OPDIPort.o ../opdi_core/code/c/common/master/opdi_OPDIPort.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_PortFactory.o: ../opdi_core/code/c/common/master/opdi_PortFactory.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_PortFactory.o ../opdi_core/code/c/common/master/opdi_PortFactory.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_ProtocolFactory.o: ../opdi_core/code/c/common/master/opdi_ProtocolFactory.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_ProtocolFactory.o ../opdi_core/code/c/common/master/opdi_ProtocolFactory.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_SelectPort.o: ../opdi_core/code/c/common/master/opdi_SelectPort.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_SelectPort.o ../opdi_core/code/c/common/master/opdi_SelectPort.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_StringTools.o: ../opdi_core/code/c/common/master/opdi_StringTools.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_StringTools.o ../opdi_core/code/c/common/master/opdi_StringTools.cpp

${OBJECTDIR}/_ext/b93babfe/opdi_main_io.o: ../opdi_core/code/c/common/master/opdi_main_io.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/b93babfe
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/b93babfe/opdi_main_io.o ../opdi_core/code/c/common/master/opdi_main_io.cpp

${OBJECTDIR}/_ext/e2dc17f3/opdi_aes.o: ../opdi_core/code/c/common/opdi_aes.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_aes.o ../opdi_core/code/c/common/opdi_aes.cpp

${OBJECTDIR}/_ext/e2dc17f3/opdi_message.o: ../opdi_core/code/c/common/opdi_message.c
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_message.o ../opdi_core/code/c/common/opdi_message.c

${OBJECTDIR}/_ext/e2dc17f3/opdi_port.o: ../opdi_core/code/c/common/opdi_port.c
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_port.o ../opdi_core/code/c/common/opdi_port.c

${OBJECTDIR}/_ext/e2dc17f3/opdi_protocol.o: ../opdi_core/code/c/common/opdi_protocol.c
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_protocol.o ../opdi_core/code/c/common/opdi_protocol.c

${OBJECTDIR}/_ext/e2dc17f3/opdi_rijndael.o: ../opdi_core/code/c/common/opdi_rijndael.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_rijndael.o ../opdi_core/code/c/common/opdi_rijndael.cpp

${OBJECTDIR}/_ext/e2dc17f3/opdi_slave_protocol.o: ../opdi_core/code/c/common/opdi_slave_protocol.c
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_slave_protocol.o ../opdi_core/code/c/common/opdi_slave_protocol.c

${OBJECTDIR}/_ext/e2dc17f3/opdi_strings.o: ../opdi_core/code/c/common/opdi_strings.c
	${MKDIR} -p ${OBJECTDIR}/_ext/e2dc17f3
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/e2dc17f3/opdi_strings.o ../opdi_core/code/c/common/opdi_strings.c

${OBJECTDIR}/_ext/a50b0fff/fifo.o: ../opdi_core/code/c/libraries/libctb/src/fifo.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/fifo.o ../opdi_core/code/c/libraries/libctb/src/fifo.cpp

${OBJECTDIR}/_ext/a50b0fff/getopt.o: ../opdi_core/code/c/libraries/libctb/src/getopt.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/getopt.o ../opdi_core/code/c/libraries/libctb/src/getopt.cpp

${OBJECTDIR}/_ext/a50b0fff/iobase.o: ../opdi_core/code/c/libraries/libctb/src/iobase.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/iobase.o ../opdi_core/code/c/libraries/libctb/src/iobase.cpp

${OBJECTDIR}/_ext/a50b0fff/kbhit.o: ../opdi_core/code/c/libraries/libctb/src/kbhit.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/kbhit.o ../opdi_core/code/c/libraries/libctb/src/kbhit.cpp

${OBJECTDIR}/_ext/f5efeae4/serport.o: ../opdi_core/code/c/libraries/libctb/src/linux/serport.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/f5efeae4
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/f5efeae4/serport.o ../opdi_core/code/c/libraries/libctb/src/linux/serport.cpp

${OBJECTDIR}/_ext/f5efeae4/timer.o: ../opdi_core/code/c/libraries/libctb/src/linux/timer.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/f5efeae4
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/f5efeae4/timer.o ../opdi_core/code/c/libraries/libctb/src/linux/timer.cpp

${OBJECTDIR}/_ext/a50b0fff/portscan.o: ../opdi_core/code/c/libraries/libctb/src/portscan.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/portscan.o ../opdi_core/code/c/libraries/libctb/src/portscan.cpp

${OBJECTDIR}/_ext/a50b0fff/serportx.o: ../opdi_core/code/c/libraries/libctb/src/serportx.cpp
	${MKDIR} -p ${OBJECTDIR}/_ext/a50b0fff
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/a50b0fff/serportx.o ../opdi_core/code/c/libraries/libctb/src/serportx.cpp

${OBJECTDIR}/_ext/1522cf7d/opdi_platformfuncs.o: ../opdi_core/code/c/platforms/linux/opdi_platformfuncs.c
	${MKDIR} -p ${OBJECTDIR}/_ext/1522cf7d
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/platforms/linux -I../opdi_core/code/c/platforms -Isrc -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/1522cf7d/opdi_platformfuncs.o ../opdi_core/code/c/platforms/linux/opdi_platformfuncs.c

${OBJECTDIR}/src/AbstractOpenHAT.o: src/AbstractOpenHAT.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/AbstractOpenHAT.o src/AbstractOpenHAT.cpp

${OBJECTDIR}/src/Configuration.o: src/Configuration.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Configuration.o src/Configuration.cpp

${OBJECTDIR}/src/ExecPort.o: src/ExecPort.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/ExecPort.o src/ExecPort.cpp

${OBJECTDIR}/src/ExpressionPort.o: src/ExpressionPort.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/ExpressionPort.o src/ExpressionPort.cpp

${OBJECTDIR}/src/LinuxOpenHAT.o: src/LinuxOpenHAT.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/LinuxOpenHAT.o src/LinuxOpenHAT.cpp

${OBJECTDIR}/src/OPDI.o: src/OPDI.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/OPDI.o src/OPDI.cpp

${OBJECTDIR}/src/OPDI_Ports.o: src/OPDI_Ports.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/OPDI_Ports.o src/OPDI_Ports.cpp

${OBJECTDIR}/src/Ports.o: src/Ports.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Ports.o src/Ports.cpp

${OBJECTDIR}/src/SunRiseSet.o: src/SunRiseSet.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/SunRiseSet.o src/SunRiseSet.cpp

${OBJECTDIR}/src/TimerPort.o: src/TimerPort.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/TimerPort.o src/TimerPort.cpp

${OBJECTDIR}/src/openhat_linux.o: src/openhat_linux.cpp
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I../opdi_core/code/c/common -I../opdi_core/code/c/common/master -I../opdi_core/code/c/libraries/POCO/Util/include -I../opdi_core/code/c/libraries/POCO/Foundation/include -I../opdi_core/code/c/libraries/POCO/Net/include -I../opdi_core/code/c/libraries/POCO/JSON/include -I../opdi_core/code/c/libraries/POCO/XML/include -I../opdi_core/code/c/platforms -I../opdi_core/code/c/platforms/linux -Isrc -I../opdi_core/code/c/libraries/libctb/include -I../libraries/exprtk -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/openhat_linux.o src/openhat_linux.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
