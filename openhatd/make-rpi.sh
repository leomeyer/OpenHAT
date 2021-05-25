#! /bin/bash

rm -rf build-rpi/
mkdir build-rpi
cd build-rpi
cmake .. -DCMAKE_TOOLCHAIN_FILE=../CMake-rpi
make release
