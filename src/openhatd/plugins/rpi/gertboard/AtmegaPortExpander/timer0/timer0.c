//    This file is part of an OPDI reference implementation.
//    see: Open Protocol for Device Interaction
//
//    Copyright (C) 2011 Leo Meyer (leo@leomeyer.de)
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
    
/************************************************************************/
/*                                                                      */
/*                  Timer0 helper routines                   			*/
/*                  Supports a clock reference and interrupt chaining.	*/
/*                                                                      */
/* Code License: GNU GPL												*/
/************************************************************************/
// Leo Meyer, 02/2010

#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "timer0.h"

static volatile struct timer0_hook {
	TIMER0_HOOK hook;
	uint16_t ticks;
	uint16_t counter;
} hooks[MAX_HOOKS];

static int8_t num_hooks = -1;
static volatile long timer0_tcount;

// Used to initialize the encoder interrupt
void timer0_init(void)
{
	// already initialized?
	if (num_hooks >= 0)
		return;
		
	num_hooks = 0;
	
	// no interrupts during setup!
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {	
  
#if defined (atmega8)
		// set preselector
		TCCR0 |= (1 << CS01) | (1 << CS00);		// CTC, XTAL / 64
		// adjust counter for 1 ms
		TCNT0 = 256 - (uint8_t)(F_OSC / 64.0 * 1e-3 - 0.5);		
		// enable interrupt on timer0 overflow
		TIMSK |= (1 << TOIE0);
#elif defined (atmega168) || defined (atmega328p)
		TCCR0A = 1 << WGM01;
		TCCR0B = (1 << CS01) | (1 << CS00);	// Prescaler 64
		OCR0A = 256 - (uint8_t)(F_OSC / 64.0 * 1e-3 - 0.5);
		TIMSK0 = 1 << OCIE0A;
#else
#.error CPU not defined
#endif
	}	// ATOMIC
	
}
 
// Interrupt service routine
ISR(TIMER0_VECTOR)					
{	
	// adjust counter for 1 ms
	// this is done first so that the timer keeps running during interrupt
	// handling, making the clock more accurate.
#if defined (atmega8)
	TCNT0 = 256 - (uint8_t)(F_OSC / 64.0 * 1e-3 - 0.5);	// 1ms
#elif defined (atmega168) || defined (atmega328p)
	OCR0A = 256 - (uint8_t)(F_OSC / 64.0 * 1e-3 - 0.5);
#else
#.error CPU not defined
#endif  

	// increase tick counter
	timer0_tcount++;
	
	uint8_t i = 0;
	// go through all hooks
	while ((hooks[i].hook != NULL) && (i < MAX_HOOKS)) {

		hooks[i].counter--;
		if (hooks[i].counter <= 0) {
			hooks[i].counter = hooks[i].ticks;
			// countdown reached, call function
			hooks[i].hook(timer0_tcount);
		}	
		i++;
	}
}

void timer0_add_hook(TIMER0_HOOK hook, uint16_t ticks) {
	// initialize if not already done
	timer0_init();

	if (hook == NULL)
		return;
	// can add hook?
	if (num_hooks >= MAX_HOOKS)
		return;
	// init hook
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {	
		hooks[num_hooks].hook = hook;
		hooks[num_hooks].ticks = ticks;
		// start countdown
		hooks[num_hooks].counter = ticks;
		num_hooks++;
	}
}

long timer0_ticks(void) {
	long result = 0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {	
		result = timer0_tcount;
	}
	return result;
 }


void timer0_delay(uint32_t ms) {
	uint32_t counter = timer0_ticks();
	while (timer0_ticks() - counter < ms);
}