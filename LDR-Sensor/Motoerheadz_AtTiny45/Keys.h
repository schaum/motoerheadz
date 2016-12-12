/*
 *  Keys.h
 *  Demo
 *
 *  Created by UrbanBieri on 13.03.10.
 *  a changed program of Peter Danegger
 *	see: http://www.mikrocontroller.net/articles/Entprellung
 *
 */
 
//#ifndef F_CPU
//#define F_CPU           1000000                   // processor clock frequency
//#warning kein F_CPU definiert
//#endif

#define KEY_DDR	        DDRB
#define KEY_PORT        PORTB
#define KEY_PIN         PINB
#define KEY0            PB3
// #define KEY1 			PB4
 // #define KEY2			PB1
#define ALL_KEYS        (1<<KEY0)

#define REPEAT_MASK     (1<<KEY0)
#define REPEAT_START    8
#define REPEAT_NEXT		64 // only first is important

void keyDebounceAndState();

//uint8_t get_key_press(uint8_t);
uint8_t get_key_release(uint8_t);
uint8_t get_key_second_press(uint8_t);
uint8_t get_key_first_press(uint8_t);
uint8_t get_key_long(uint8_t);

void initKeys();
