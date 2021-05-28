cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(GERTBOARD ${OPENHAT_LIBARIES}/rpi/gertboard)

include(../../CMakeLists.inc)

target_compile_options(${PROJECT_NAME} 
    PRIVATE 
    "-Wno-pointer-sign" "-Wno-unused-but-set-variable" "-DCMAKE_BUILD=1"
    )

target_sources(${PROJECT_NAME}
    PRIVATE
        ${GERTBOARD}/gb_pwm.c
        ${GERTBOARD}/gb_spi.c
        ${GERTBOARD}/gb_common.c
    )

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${GERTBOARD}
    )
