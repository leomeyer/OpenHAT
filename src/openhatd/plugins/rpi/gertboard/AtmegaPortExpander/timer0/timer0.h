//    This file is part of an OPDI reference implementation.
//    see: Open Protocol for Device Interaction
//
//    Copyright (C) 2009 - 2011 Leo Meyer (leo@leomeyer.de)
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
#ifndef _TIMER0_H___
#define _TIMER0_H___

/************************************************************************/
/*                                                                      */
/*                  Timer0 helper routines                   			*/
/*                  Supports a clock reference and interrupt chaining.	*/
/*                  This library is the basis for several other 		*/
/*					components such as controls, encoder, segdisp etc.	*/
/* Code License: GNU GPL												*/
/************************************************************************/
// Leo Meyer, 02/2010

#include <stdint.h>

// Specifies the maximum number of hooks that can be added.
// Keep the number low to conserve RAM. Each hook uses 6 bytes.
// The following modules may use one hook each:
// - rotary encoder
// - control buttons
// - segment display
// If you are using a module that requires a hook but there is no hook free,
// nothing will probably happen except that the module will not work.
#define MAX_HOOKS	3

// Timer and interrupt settings
// Add the settings of your controller here.
// Additional CPU specific settings must be inserted in the .c file.

#if defined (atmega8)
	#define TIMER0_VECTOR		TIMER0_OVF_vect
#elif defined (atmega168)
	#define TIMER0_VECTOR		TIMER0_COMPA_vect
#elif defined (atmega328p)
	#define TIMER0_VECTOR		TIMER0_COMPA_vect
#else
	#error "CPU type not recognized, use CDEFS=-D$(MCU) in the makefile"
#endif

// Interrupt hook function type
typedef void (*TIMER0_HOOK)(long ticks);

// Initializes the timer.
// Interrupts are not automatically enabled in this routine.
void timer0_init(void);

// Adds the given hook for the specified number of ticks
// (hook is called every <ticks> ms).
// The hook is a normal function of type TIMER0_HOOK,
// not an interrupt service routine or something.
// A value of NULL is invalid.
// If the timer0 hasn't been initialized yet, this is being done.
void timer0_add_hook(TIMER0_HOOK hook, uint16_t ticks);

// Returns the number of ticks since the start of the timer.
// This represents a fairly accurate 1ms clock.
long timer0_ticks(void);

// Busy waits until the ms have elapsed.
// If the timer has not been initialized before, this routine hangs
// indefinitely.
void timer0_delay(uint32_t ms);

#endif
