cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(GERTBOARD ${OPENHAT_LIBARIES}/rpi/gertboard)

include(../../CMakeLists.inc)

target_sources(${PROJECT_NAME}
    PUBLIC
        ${GERTBOARD}/gb_pwm.c
        ${GERTBOARD}/gb_spi.c
        ${GERTBOARD}/gb_common.c
    )

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${GERTBOARD}
    )