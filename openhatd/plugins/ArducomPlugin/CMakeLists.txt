cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(ARDUCOM ${OPENHAT_LIBARIES}/Arducom/src/master)

include(../CMakeLists.inc)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${DIR_LEVEL}/openhatd/src/ExpressionPort.cpp
        ${ARDUCOM}/ArducomMaster.cpp
        ${ARDUCOM}/ArducomMasterSerial.cpp
        ${ARDUCOM}/ArducomMasterTCPIP.cpp
        ${ARDUCOM}/ArducomMasterI2C.cpp
    )

target_link_libraries(${PROJECT_NAME} libcrypto.a)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${ARDUCOM}
    )
