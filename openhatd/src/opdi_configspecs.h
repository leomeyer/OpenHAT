//    Copyright (C) 2011-2014 OpenHAT contributors (https://openhat.org, https://github.com/openhat-org)
//

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    
// OpenHAT config specifications

#ifndef __OPDI_CONFIGSPECS_H
#define __OPDI_CONFIGSPECS_H

#include "opdi_platformtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// This config is a slave
#define OPDI_IS_SLAVE	1

// Sets the default encoding that is used by this config.
#define OPDI_ENCODING_DEFAULT		OPDI_ENCODING_UTF8

// Defines the maximum message length this config can receive.
// Consumes this amount of bytes in RAM.
#define OPDI_MESSAGE_BUFFER_SIZE		65535

// Defines the maximum message string length this config can receive.
// Consumes this amount of bytes in RAM.
// As the channel identifier and the checksum of a message typically consume a
// maximum amount of nine bytes, this value may be OPDI_MESSAGE_BUFFER_SIZE - 9
// on systems with only single-byte character sets.
#define OPDI_MESSAGE_PAYLOAD_LENGTH	(OPDI_MESSAGE_BUFFER_SIZE - 9)

// maximum permitted message parts
#define OPDI_MAX_MESSAGE_PARTS	16

// maximum possible ports on this device
#define OPDI_MAX_DEVICE_PORTS	4096

// define to conserve RAM and ROM
//#define OPDI_NO_DIGITAL_PORTS

// define to conserve RAM and ROM
//#define OPDI_NO_ANALOG_PORTS

// define to conserve RAM and ROM
// #define OPDI_NO_SELECT_PORTS

// define to conserve RAM and ROM
// #define OPDI_NO_DIAL_PORTS
    
#define OPDI_USE_CUSTOM_PORTS

// Defines the number of possible streaming ports on this device.
// May be set to 0 to conserve memory.
#define OPDI_STREAMING_PORTS		0

// define to conserve memory
// #define OPDI_NO_ENCRYPTION

// define to conserve memory
// #define OPDI_NO_AUTHENTICATION

// this device supports the extended protocol
#define OPDI_EXTENDED_PROTOCOL			1

// extended protocol info buffer (on stack)
// used for extended device info and extended port state
#define OPDI_EXTENDED_INFO_LENGTH		4096

/** Defines the block size of data for encryption. Depends on the encryption implementation.
*   Data is always sent to the encrypt_block function in blocks of this size. If necessary, it is
*   padded with random bytes. When receiving data, the receiving function waits until a full block has 
*   been received before passing it to the decrypt_block function.
*   A maximum of 2^^16 - 1 is supported.
*/
#define OPDI_ENCRYPTION_BLOCKSIZE	16

#define OPDI_HAS_MESSAGE_HANDLED

// keep these numbers as low as possible to conserve memory
#define OPDI_MAX_PORTIDLENGTH		64
#define OPDI_MAX_PORTNAMELENGTH		64
#define OPDI_MAX_SLAVENAMELENGTH	64
#define OPDI_MAX_ENCODINGNAMELENGTH	32

#define OPDI_MAX_PORT_INFO_MESSAGE	1024

// To be able to use the ExpressionPort define this macro. The ../../../libraries/ExprTk folder must contain the
// file exprtk.hpp.
#define OPENHAT_USE_EXPRTK			1

#ifdef __cplusplus
}
#endif


#endif		// __CONFIGSPECS_H
