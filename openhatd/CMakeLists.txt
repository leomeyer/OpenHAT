cmake_minimum_required(VERSION 3.0.0)
project(openhatd 
    VERSION 0.1.0
    DESCRIPTION "OpenHAT Automation Server"
    LANGUAGES C CXX)

set(SRC src)
set(OPDI_CORE ../opdi_core)
set(OPDI_CODE ${OPDI_CORE}/code/c)
set(OPDI_LIBRARIES ${OPDI_CODE}/libraries)
set(OPDI_LIBCTB ${OPDI_LIBRARIES}/libctb)
set(OPDI_POCO ${OPDI_LIBRARIES}/POCO)
set(OPDI_POCO_FOUNDATION ${OPDI_POCO}/Foundation)
set(OPDI_POCO_NET ${OPDI_POCO}/Net)
set(OPDI_POCO_XML ${OPDI_POCO}/XML)

set(OPENHAT_LIBRARIES ../libraries)

set(OPDI_COMMON ${OPDI_CODE}/common)
set(OPDI_PLATFORMS ${OPDI_CODE}/platforms)
set(OPDI_PLATFORMS_LINUX ${OPDI_PLATFORMS}/linux)

set(OPENHAT_EXPRTK ${OPENHAT_LIBRARIES}/exprtk)

set(HEADER_FILES 
    ${OPDI_COMMON}/opdi_config.h
    ${OPDI_COMMON}/opdi_constants.h
    ${OPDI_COMMON}/opdi_message.h
    ${OPDI_COMMON}/opdi_port.h
    ${OPDI_COMMON}/opdi_protocol.h
    ${OPDI_COMMON}/opdi_protocol_constants.h
    ${OPDI_COMMON}/opdi_rijndael.h
    ${OPDI_COMMON}/opdi_slave_protocol.h
    ${OPDI_COMMON}/opdi_strings.h

    ${OPDI_PLATFORMS}/opdi_platformfuncs.h
    ${OPDI_PLATFORMS_LINUX}/opdi_platformtypes.h
    
    src/opdi_configspecs.h
    )

add_executable(openhatd 
    ${OPDI_LIBCTB}/src/fifo.cpp
    ${OPDI_LIBCTB}/src/getopt.cpp
    ${OPDI_LIBCTB}/src/iobase.cpp
    ${OPDI_LIBCTB}/src/kbhit.cpp
    ${OPDI_LIBCTB}/src/portscan.cpp
    ${OPDI_LIBCTB}/src/serportx.cpp
    ${OPDI_LIBCTB}/src/linux/serport.cpp
    ${OPDI_LIBCTB}/src/linux/timer.cpp

    ${OPDI_COMMON}/opdi_aes.cpp
    ${OPDI_COMMON}/opdi_message.c
    ${OPDI_COMMON}/opdi_port.c
    ${OPDI_COMMON}/opdi_protocol.c
    ${OPDI_COMMON}/opdi_rijndael.cpp
    ${OPDI_COMMON}/opdi_slave_protocol.c
    ${OPDI_COMMON}/opdi_strings.c
    ${OPDI_PLATFORMS_LINUX}/opdi_platformfuncs.c

    ${SRC}/AbstractOpenHAT.cpp
    ${SRC}/Configuration.cpp
    ${SRC}/ExecPort.cpp
    ${SRC}/ExpressionPort.cpp
    ${SRC}/LinuxOpenHAT.cpp
    ${SRC}/openhat_linux.cpp
    ${SRC}/OPDI_Ports.cpp
    ${SRC}/OPDI.cpp
    ${SRC}/Ports.cpp
    ${SRC}/SunRiseSet.cpp
    ${SRC}/TimerPort.cpp
    )

target_include_directories(openhatd PRIVATE 
    ${SRC}
    ${OPDI_COMMON}
    ${OPDI_PLATFORMS}
    ${OPDI_PLATFORMS_LINUX}
    ${OPDI_LIBCTB}/include
    ${OPENHAT_EXPRTK}
    )

target_link_libraries(openhatd -L${OPDI_POCO}/lib/Linux/x86_64/)
target_link_libraries(openhatd -ldl -lpthread)
target_link_libraries(openhatd PocoFoundation)
target_link_libraries(openhatd PocoUtil)
target_link_libraries(openhatd PocoNet)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
