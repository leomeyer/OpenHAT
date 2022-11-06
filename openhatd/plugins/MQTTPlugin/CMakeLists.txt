cmake_minimum_required(VERSION 3.0.0)

set(DIR_LEVEL ../../..)

set(OPENHAT_LIBARIES ${DIR_LEVEL}/libraries)

set(PAHO_MQTT_C ${OPENHAT_LIBARIES}/paho.mqtt.c)
set(PAHO_MQTT_CPP ${OPENHAT_LIBARIES}/paho.mqtt.cpp)

set(PAHO_MQTT_C_INCLUDE ${PAHO_MQTT_C}/src)
set(PAHO_MQTT_CPP_INCLUDE ${PAHO_MQTT_CPP}/src)
set(PAHO_MQTT_C_LIB ${PAHO_MQTT_C}/${BUILDNAME}/src)
set(PAHO_MQTT_CPP_LIB ${PAHO_MQTT_CPP}/${BUILDNAME}/src)

link_directories(${PAHO_MQTT_C_LIB} ${PAHO_MQTT_CPP_LIB})

include(../CMakeLists.inc)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${DIR_LEVEL}/openhatd/src/ExpressionPort.cpp
    )

target_link_libraries(${PROJECT_NAME} libpaho-mqttpp3.a)
target_link_libraries(${PROJECT_NAME} libpaho-mqtt3a.a)
target_link_libraries(${PROJECT_NAME} libpaho-mqtt3c.a)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${PAHO_MQTT_C_INCLUDE}
    ${PAHO_MQTT_CPP_INCLUDE}
    )
