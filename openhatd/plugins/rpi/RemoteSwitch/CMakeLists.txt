cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(RASPBERRY_REMOTE ${OPENHAT_LIBARIES}/rpi/raspberry-remote)
set(WIRING_PI ${OPENHAT_LIBARIES}/rpi/wiringPi/wiringPi)
set(GERTBOARD ${OPENHAT_LIBARIES}/rpi/gertboard)

add_compile_options("-fpermissive")

include(../../CMakeLists.inc)

target_sources(${PROJECT_NAME}
    PUBLIC
        ${RASPBERRY_REMOTE}/RCSwitch.cpp
        ${WIRING_PI}/wiringPi.c
        ${GERTBOARD}/gb_pwm.c
        ${GERTBOARD}/gb_spi.c
    )

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${WIRING_PI}
    ${RASPBERRY_REMOTE}
    )