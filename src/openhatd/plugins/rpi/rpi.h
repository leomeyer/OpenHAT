#ifndef __RPI_H
#define  __RPI_H

// define the common prefix for reserving GPIO resources in all Raspberry Pi plugins
// call AbstractOpenHAT::lockResources with this prefix followed by the GPIO pin number.

#define RPI_GPIO_PREFIX		std::string("GPIO@")

#endif