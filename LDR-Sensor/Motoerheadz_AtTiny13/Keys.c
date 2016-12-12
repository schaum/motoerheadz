/*
 *  Keys.c
 *  Demo
 *
 *  Created by UrbanBieri on 16.09.16.
 *  a changed class of Peter Danegger 
 *	see: http://www.mikrocontroller.net/articles/Entprellung
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "Keys.h"

// #define LED (1<<PINB0) // for debugging
 
volatile uint8_t key_state;			// debounced and inverted key state: bit = 1: key pressed
// volatile uint8_t key_press;			// key press detect
volatile uint8_t key_release;		// key release detect
volatile uint8_t key_long;			// key long press
volatile uint8_t key_second_press;	// press first a butten, before releasing press the second
 
 
void keyDebounceAndState()          // invoke every 20ms or more to ensure debouncing
									// see ISR in main-class
{
	static uint8_t repeat, repeatON;
	static uint8_t enableSimpleKeys = 1;
	uint8_t i, new;
 
	i = key_state ^ (~KEY_PIN & ALL_KEYS);					// key changed ?
	key_state ^= i; 							// then toggle state
	new = key_state & i;						// only newly pressed buttons
	
	if((key_state==0) && (i==0)) {
		enableSimpleKeys = 1;
	}

	// if there is a new button pressed, and one without the new ones too (an old one)
	if((new != 0 ) && ((key_state & ~new) != 0)){
 		key_second_press |= new;				// add only new ones 		
	// disable key_release after second_press to be shure that only the seconds are counted, 
	// enable again if no button is pressed or released (see above)
 		enableSimpleKeys = 0;
 	}
	// key_press |= new;						// 0->1: key press detect
	// nicht bei komplizierteren Keys, auch auf REPEAT checken, damit auf den Repeat kein RELEASE folgt
 	if (enableSimpleKeys == 1 && repeatON == 0) {
 		key_release |= ~key_state & i;			// 1->0: key release detect
	} 

	// check repeat function
	if( ((key_state & REPEAT_MASK) == 0) || (enableSimpleKeys == 0)) {		// if no repeat-key is pressed
		repeat = REPEAT_START;					// start delay
		repeatON = 0;
	}
	if(--repeat == 0){
		repeat = REPEAT_NEXT;					// repeat delay
		key_long |= key_state & REPEAT_MASK;
		repeatON = 1;
	}

}

/*
///////////////////////////////////////////////////////////////////
//
// check if a key has been pressed. Each pressed key is cleared
//
uint8_t get_key_press(uint8_t key_mask){
	cli();                                          // read and clear atomic !
	key_mask &= key_press;                          // read key(s)
	key_press ^= key_mask;                          // clear key(s)
	sei();
	return key_mask;
}
*/

///////////////////////////////////////////////////////////////////
//
// check if a key has been released. Each released key is cleared
//
uint8_t get_key_release(uint8_t key_mask){
	cli();                                          // read and clear atomic !
	key_mask &= key_release;                        // read key(s)
	key_release ^= key_mask;                        // clear key(s)
	sei();
	return key_mask;

}

///////////////////////////////////////////////////////////////////
//
// if more than one button is pressed, return the last one
//
uint8_t get_key_second_press(uint8_t key_mask){				
	cli();                                          // read and clear atomic !
	key_mask &= key_second_press;                   // read key(s)
	key_second_press ^= key_mask;                   // clear key(s)
	sei();
	return key_mask;

}
///////////////////////////////////////////////////////////////////
//
// maybe you want to know which are the first buttons if two or more buttons are pressed.
// in that case, these first buttons should not be cleared (by using get_key_press()) 
// to ensure that the second button, repeatedly pressed, stays recognized as the second button.
//
uint8_t get_key_first_press(uint8_t key_mask){
	cli();
	key_mask &= key_state & ~key_second_press;		// return first button(s) without clearing it!
	sei();
	return key_mask;
}
 

///////////////////////////////////////////////////////////////////
//
// check if a key has been pressed long enough such that the
// key repeat functionality kicks in. After a small setup delay
// the key is reported beeing pressed in subsequent calls
// to this function. This simulates the user repeatedly
// pressing and releasing the key.
//
uint8_t get_key_long(uint8_t key_mask){
	cli();                                          // read and clear atomic !
	key_mask &= key_long;                            // read key(s)
	key_long ^= key_mask;                            // clear key(s)
	sei();
	return key_mask;
}

 
///////////////////////////////////////////////////////////////////
//set KEY0 as input (to 0) and activate pullup resistor
void initKeys()
{
	KEY_DDR &= ~ALL_KEYS;                // configure key port for input
	KEY_PORT |= ALL_KEYS;                // set pull up resistors
}

