cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(MONGOOSE ${OPENHAT_LIBARIES}/Mongoose)

include(../CMakeLists.inc)

target_compile_options(${PROJECT_NAME} 
    PRIVATE 
    "-Wno-format-truncation"
    )

target_sources(${PROJECT_NAME}
    PRIVATE
        ${MONGOOSE}/mongoose.c
    )

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${MONGOOSE}
    )