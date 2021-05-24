cmake_minimum_required(VERSION 3.0.0)

project(OpenHAT-Plugins)

add_subdirectory(FritzBoxPlugin)
add_subdirectory(FroniusPlugin)
add_subdirectory(linux_test)
add_subdirectory(MQTTPlugin)
if(OPENHAT_TARGET STREQUAL armhf)
    add_subdirectory(rpi/gertboard)
#    add_subdirectory(rpi/RemoteSwitch)
endif()
add_subdirectory(WeatherPlugin)
add_subdirectory(WebServerPlugin)
add_subdirectory(WindowPlugin)

add_custom_target(${PROJECT_NAME}
    DEPENDS FritzBoxPlugin FroniusPlugin linux_test MQTTPlugin WeatherPlugin 
            WebServerPlugin WindowPlugin gertboard) # RemoteSwitch
