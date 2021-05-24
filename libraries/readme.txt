

Building Paho static libraries
------------------------------

Windows
*******

- Download Ninja from https://github.com/ninja-build/ninja, install ninja.exe in C:\Windows\system32
- In VS x86 native tools command prompt:
	paho.mqtt.c> cmake -DPAHO_BUILD_STATIC=TRUE -DPAHO_BUILD_SAMPLES=FALSE -GNinja .
    paho.mqtt.c> ninja
	
	paho.mqtt.cpp> cmake -GNinja -DPAHO_MQTT_C_LIBRARIES=..\paho.mqtt.c\src -DPAHO_MQTT_C_INCLUDE_DIRS=..\paho.mqtt.c\src
	paho.mqtt.cpp> ninja


Linux
*****

- Install cmake and Ninja: $ sudo apt install cmake ninja-build
- Configure and build:
    paho.mqtt.c> cmake -Bbuild -H. -DPAHO_WITH_SSL=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_ENABLE_TESTING=OFF
    paho.mqtt.c> cmake --build build

    paho.mqtt.cpp> mkdir build && cd build
    paho.mqtt.cpp> cmake -DPAHO_MQTT_C_LIBRARIES=../../paho.mqtt.c/build/src -DPAHO_MQTT_C_INCLUDE_DIRS=../../paho.mqtt.c/src -DPAHO_BUILD_STATIC=TRUE -DPAHO_BUILD_SHARED=FALSE ../
    paho.mqtt.cpp> make


Cross-compiling for Raspberry Pi
********************************

    paho.mqtt.c> cmake -Bbuild-rpi -H. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_SHARED=OFF -DPAHO_ENABLE_TESTING=OFF -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc
    paho.mqtt.c> cmake --build build-rpi/

    paho.mqtt.cpp> cmake -Bbuild-rpi -H. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_SHARED=OFF -DPAHO_MQTT_C_LIBRARIES=../paho.mqtt.c/build-rpi/src -DPAHO_MQTT_C_INCLUDE_DIRS=../paho.mqtt.c/src -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
    paho.mqtt.cpp> cmake --build build-rpi/

