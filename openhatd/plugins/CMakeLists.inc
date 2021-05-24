# to be included by plugins
set(OPENHAT_SRC ${DIR_LEVEL}/openhatd/src)

get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})

file (STRINGS ${OPENHAT_SRC}/VERSION RawVersion)
string(REPLACE "\"" "" Version ${RawVersion})

project(${ProjectId}
    VERSION ${Version}
    LANGUAGES C CXX)

set(OPDI_CORE ${DIR_LEVEL}/opdi_core)
set(OPDI_CODE ${OPDI_CORE}/code/c)
set(OPDI_LIBRARIES ${OPDI_CODE}/libraries)
set(OPDI_LIBCTB ${OPDI_LIBRARIES}/libctb)
set(OPDI_POCO ${OPDI_LIBRARIES}/POCO)
set(OPDI_POCO_FOUNDATION ${OPDI_POCO}/Foundation)
set(OPDI_POCO_UTIL ${OPDI_POCO}/Util)
set(OPDI_POCO_NET ${OPDI_POCO}/Net)
set(OPDI_POCO_XML ${OPDI_POCO}/XML)
set(OPDI_POCO_JSON ${OPDI_POCO}/JSON)

if (NOT DEFINED POCO_LIBRARIES)
    set(POCO_LIBRARIES ${OPDI_POCO}/lib/Linux/x86_64/)
else()
    set(POCO_LIBRARIES ${OPDI_POCO}/lib/${POCO_LIBRARIES})
endif()
set(OPENHAT_LIBRARIES ${DIR_LEVEL}/libraries)

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
    )

link_directories(${POCO_LIBRARIES})

add_link_options("-Wl,-z,defs")

add_library(${PROJECT_NAME} SHARED
    ${OPDI_PLATFORMS_LINUX}/opdi_platformfuncs.c

    ${OPENHAT_SRC}/Configuration.cpp
    ${OPENHAT_SRC}/OPDI_Ports.cpp

    ${PROJECT_NAME}.cpp
    )

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${OPENHAT_SRC}
    ${OPDI_COMMON}
    ${OPDI_PLATFORMS}
    ${OPDI_PLATFORMS_LINUX}
    ${OPENHAT_EXPRTK}
    ${OPDI_LIBCTB}/include
    ${OPDI_POCO_UTIL}/include
    ${OPDI_POCO_NET}/include
    ${OPDI_POCO_XML}/include
    ${OPDI_POCO_JSON}/include
    ${OPDI_POCO_FOUNDATION}/include
    )

target_link_libraries(${PROJECT_NAME} -ldl -lpthread)
target_link_libraries(${PROJECT_NAME} libPocoUtil.a)
target_link_libraries(${PROJECT_NAME} libPocoNet.a)
target_link_libraries(${PROJECT_NAME} libPocoXML.a)
target_link_libraries(${PROJECT_NAME} libPocoJSON.a)
target_link_libraries(${PROJECT_NAME} libPocoFoundation.a)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    )

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}
)