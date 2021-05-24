cmake_minimum_required(VERSION 3.0.0)

project(OpenHAT)

get_filename_component(BUILDNAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

if(OPENHAT_TARGET STREQUAL "")
    set(OPENHAT_TARGET linux)
endif()

file (STRINGS src/VERSION RawVersion)
string(REPLACE "\"" "" OPENHAT_VERSION ${RawVersion})

add_subdirectory(plugins)
add_subdirectory(src)

add_custom_target(release
    DEPENDS openhatd OpenHAT-Plugins)

add_custom_command(TARGET release
    COMMAND ../release.sh "${OPENHAT_TARGET}-${OPENHAT_VERSION}"
)