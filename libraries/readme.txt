

Building Paho libraries
-----------------------

Windows
*******

- Download Ninja from https://github.com/ninja-build/ninja, install ninja.exe in C:\Windows\system32
- In VS x86 native tools command prompt:
	paho.mqtt.c> cmake -DPAHO_BUILD_STATIC=TRUE -DPAHO_BUILD_SAMPLES=FALSE -GNinja .
    paho.mqtt.c> ninja
	
	paho.mqtt.cpp> cmake -GNinja -DPAHO_MQTT_C_LIBRARIES=..\paho.mqtt.c\src -DPAHO_MQTT_C_INCLUDE_DIRS=..\paho.mqtt.c\src
	paho.mqtt.cpp> ninja

