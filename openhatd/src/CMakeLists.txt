cmake_minimum_required(VERSION 3.0.0)

set(SRC .)

file (STRINGS ${SRC}/VERSION RawVersion)
string(REPLACE "\"" "" Version ${RawVersion})

project(openhatd
    VERSION ${Version}
    DESCRIPTION "OpenHAT Automation Server"
    LANGUAGES C CXX)

set(OPDI_CORE ../../opdi_core)
set(OPDI_CODE ${OPDI_CORE}/code/c)
set(OPDI_LIBRARIES ${OPDI_CODE}/libraries)
set(OPDI_LIBCTB ${OPDI_LIBRARIES}/libctb)
set(OPDI_POCO ${OPDI_LIBRARIES}/POCO)
set(OPDI_POCO_FOUNDATION ${OPDI_POCO}/Foundation)
set(OPDI_POCO_UTIL ${OPDI_POCO}/Util)
set(OPDI_POCO_NET ${OPDI_POCO}/Net)
set(OPDI_POCO_XML ${OPDI_POCO}/XML)

if (NOT DEFINED POCO_LIBRARIES)
    set(POCO_LIBRARIES ${OPDI_POCO}/lib/Linux/x86_64/)
else()
    set(POCO_LIBRARIES ${OPDI_POCO}/lib/${POCO_LIBRARIES})
endif()
set(OPENHAT_LIBRARIES ../../libraries)

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

link_directories(${POCO_LIBRARIES})

add_executable(${PROJECT_NAME} 
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

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SRC}
    ${OPDI_COMMON}
    ${OPDI_PLATFORMS}
    ${OPDI_PLATFORMS_LINUX}
    ${OPDI_LIBCTB}/include
    ${OPENHAT_EXPRTK}
    ${OPDI_POCO_FOUNDATION}/include
    ${OPDI_POCO_UTIL}/include
    ${OPDI_POCO_NET}/include
    ${OPDI_POCO_XML}/include
    )

target_link_libraries(${PROJECT_NAME} -ldl -lpthread)
target_link_libraries(${PROJECT_NAME} PocoNet)
target_link_libraries(${PROJECT_NAME} PocoUtil)
target_link_libraries(${PROJECT_NAME} PocoFoundation)

target_compile_options(${PROJECT_NAME} 
    PRIVATE 
    "-Wall" "-Wextra" "-Wno-narrowing" "-Wno-psabi"
    "-Wno-unused-parameter" "-Wno-unused-function" "-Wno-unused-variable"
    "-Wno-sign-compare"
    )

add_custom_command(TARGET openhatd
    COMMAND cp openhatd ../..
)
