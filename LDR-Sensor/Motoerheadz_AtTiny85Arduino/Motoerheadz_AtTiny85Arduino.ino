/*************************************************************************

   chainable clock-rhythm generator

   attiny13 rhythm experiments

   15.9.16 Urban Bieri (schaum)

*************************************************************************/
/*************************************************************************

   Hardware
   
   prozessor:   ATtin25/45/85
   clock:      9.6 Mhz internal oscillator

   PB1-2 output for motor direct
   PB0 LED
   PB4 Button
   PB3 Button

*************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>


#define KEY_DDR          DDRB
#define KEY_PORT        PORTB
#define KEY_PIN         PINB
#define KEY0            PB3
#define ALL_KEYS        (1<<KEY0)

#define REPEAT_MASK     (1<<KEY0)
#define REPEAT_START    8
#define REPEAT_NEXT   64 // only first is important
 
volatile uint8_t key_state;     // debounced and inverted key state: bit = 1: key pressed
// volatile uint8_t key_press;      // key press detect
volatile uint8_t key_release;   // key release detect
volatile uint8_t key_long;      // key long press
volatile uint8_t key_second_press;  // press first a butten, before releasing press the second
 
 
void keyDebounceAndState()          // invoke every 20ms or more to ensure debouncing
                  // see ISR in main-class
{
  static uint8_t repeat, repeatON;
  static uint8_t enableSimpleKeys = 1;
  uint8_t i;
  uint8_t newV;
 
  i = key_state ^ (~KEY_PIN & ALL_KEYS);          // key changed ?
  key_state ^= i;               // then toggle state
  newV = key_state & i;            // only newly pressed buttons
  
  if((key_state==0) && (i==0)) {
    enableSimpleKeys = 1;
  }

  // if there is a new button pressed, and one without the new ones too (an old one)
  if((newV != 0 ) && ((key_state & ~newV) != 0)){
    key_second_press |= newV;        // add only new ones    
  // disable key_release after second_press to be shure that only the seconds are counted, 
  // enable again if no button is pressed or released (see above)
    enableSimpleKeys = 0;
  }
  // key_press |= new;            // 0->1: key press detect
  // nicht bei komplizierteren Keys, auch auf REPEAT checken, damit auf den Repeat kein RELEASE folgt
  if (enableSimpleKeys == 1 && repeatON == 0) {
    key_release |= ~key_state & i;      // 1->0: key release detect
  } 

  // check repeat function
  if( ((key_state & REPEAT_MASK) == 0) || (enableSimpleKeys == 0)) {    // if no repeat-key is pressed
    repeat = REPEAT_START;          // start delay
    repeatON = 0;
  }
  if(--repeat == 0){
    repeat = REPEAT_NEXT;         // repeat delay
    key_long |= key_state & REPEAT_MASK;
    repeatON = 1;
  }

}

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
  key_mask &= key_state & ~key_second_press;    // return first button(s) without clearing it!
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


// main code pins
#define MOTOROUT ((1<<PINB4)|(1<<PINB1)|(1<<PINB0))
#define BUTTON (1<<PINB3)
#define SENSOR (1<<PINB2)

// clock ticks
#define CLOCK           (1<<0)
#define CLOCK_SLOW      (1<<1)
#define CLOCK_EXTERNAL  (1<<2)

// main stati
#define RYTHM_ON        (1<<4)
#define BANG      (1<<5)
#define CALCULATE (1<<6)

// initial values
#define BANGDURATION 16
#define MINIMUM_RYTHM 90

volatile uint8_t timerDivider = 1;
volatile uint8_t ticker = 0;

void init(){
   DDRB |= MOTOROUT; // motor as output
   
   DDRB &= ~(SENSOR); //set SENSOR pin as input
// Enable ADC: ADEN to 1.
   ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // Prescaler 64=150khz Sampling 
   
   initKeys();

   /* System-clock (counter0) ------------------------------------------------- 
   * configure the counter0 for accurate System clock applications: 
   * seconds, Tastenentprellung, delay
   */
   /* set comparematch of counter0, so every 10 ms an interrupt is made.
   CTC-Modus and comparematch (reset): clock = 3686400, prescaler 256, clear timer on 144:
   3686400/256/144=100Hz -> Interrupt every 10 ms.
   */
   /* Clear Timer on Compare Match Mode (CTC): WGM01 set to 1, WGM00 set to 0.
   Prescaler 1024: CS02, CS00 set to 1
   Prescaler 256: CS02 set to 1
   */
   TCCR0A=(1<<WGM01); //CTC mode for clock modulation
   TCCR0B=(1<<CS00); // CS00 -> no prescaling. CS01 -> prescaling of 8
   // set output compare register, in this mode: resets the counter to 0.
   OCR0A=128; // in CTC Mode the counter counts up to OCR0A
   // Counter0 Output Compare Match Interrupt Enable
   TIMSK |= (1 << OCIE0A);

   // global interrupt enable
   sei();
}

// gets value from LSR (light sensitive resistor)
uint8_t start_conversion(void){
   // left Adjustet representation (ADLAR) (read only ADCH for 8-bit precision)
   // REFS0=0: reference voltage VCC
   ADMUX = (1 << ADLAR) | (0 << REFS0) | 0x01; // 0x01 is PB2, 0x10 is PB4, 0x11 is PB3.
   ADCSRA |= (1 << ADSC); // Start conversion
   while (!(ADCSRA & (1 << ADIF))); // wait for conversion complete  
   ADCSRA |= (1 << ADIF); // clear ADCIF
   return(ADCH);
}

// invoked every ??? milliseconds
ISR(TIM0_COMPA_vect) {
   timerDivider--;
   if (timerDivider==0){
      timerDivider=255;
      // Tasten Entprellung und -logik, ca alle 20-60ms
      keyDebounceAndState();
      ticker |= CLOCK_SLOW;
   }
   if ((timerDivider % 16) == 0) {
         ticker |= CLOCK;
   }
}

void setup() {
}

void loop() {
   // Initiation
   init();

   uint16_t actualRythm = 320; // set rythm-time
   uint16_t rythmCounter = 0; // dont change manually!
   uint16_t rythmCounter2 = 0;
   uint16_t rythmToSet = actualRythm;

   uint8_t bangTime = BANGDURATION;
   uint16_t rythmMeasurement = 0;
   uint8_t multiplicator = 1;
   uint8_t divisor = 1;

   uint8_t status = 0;
   status |= RYTHM_ON; // start with second rythm

   #define RYTHM_COUNT 3
   uint8_t multiplicatorArray[RYTHM_COUNT] = {1, 2, 3};
   uint8_t divisorArray[RYTHM_COUNT] =       {3, 3, 4};
// sample array    {1, 1, 1, 2, 3, 4, 3, 2, 3, 4};
   uint8_t synchronizer = multiplicator;

   uint8_t sensorValue = 0;
   uint8_t sensorMin = 255;
   uint8_t sensorMax = 0;

   while(1)
   {
      // systemtick from interrupt -> stable clock
      if(ticker & CLOCK){
         ticker &= ~(CLOCK);

         rythmCounter++;
         rythmCounter2++;
         rythmMeasurement++;

         // check from rythm if it's time for rythm-change
         if(status & CALCULATE){
            status &= ~(CALCULATE);
            rythmToSet = actualRythm / divisor;
            rythmToSet = rythmToSet * multiplicator;
         }

         // check if it's time to start a bang
            if(rythmCounter2 >= rythmToSet){
               rythmCounter2 = 0;

               if(status & RYTHM_ON){
                  status |= BANG;
                  // start motor to bang
                  PORTB |= MOTOROUT;
               }    
            }

         if(rythmCounter >= actualRythm){
            rythmCounter = 0;
            
            status |= BANG;
            // start motor to bang
            PORTB |= MOTOROUT;

            // // synchronize, comment it out if you want to float over two rythms...
            // if(--synchronizer == 0){
            //    rythmCounter2 = 0;
            //    synchronizer=multiplicator;
            // }
         }

         // check if its time to stop the motor
         if(status & BANG){
            if(--bangTime == 0){
               status &= ~(BANG);
               PORTB &= ~(MOTOROUT);
               // reset bangTime
               bangTime = BANGDURATION + (rythmToSet >> 4);
            }
         }
      }

      if(ticker & CLOCK_SLOW) {
         ticker &= ~(CLOCK_SLOW);

         // check all button-possibilities
         if(get_key_release(1<<KEY0)) {
            actualRythm = rythmMeasurement;
            rythmMeasurement = 0;
            // synchronize also rythm... reset and BANG through an overflow of rythmCounter
            rythmCounter=16000;
            status |= CALCULATE;

         }
         if(get_key_long(1<<KEY0)) {
               // toggle activation of second rythm
            status ^= RYTHM_ON;
         }

         sensorValue = start_conversion();
         // calibrate: get highest and lowest values
         if (sensorValue < sensorMin) sensorMin = sensorValue;
         if (sensorValue > sensorMax) sensorMax = sensorValue;

         sensorValue-=sensorMin;
         // for calculation position *255
         uint16_t position = (RYTHM_COUNT) << 8;
         position = (position/(sensorMax-sensorMin)*sensorValue) >> 8;
         // get divisors
         multiplicator = multiplicatorArray[position];
         divisor = divisorArray[position];

         status |= CALCULATE;
      }
	}
}

/***************************************************************************
*   
*   (c) 2016 Urban Bieri
*
***************************************************************************
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation version 3 of the License,                *
*   If you extend the program please maintain the list of authors.        *
*   If you want to use this software for commercial purposes and you      *
*   don't want to make it open source, please contact the authors for     *
*   licensing.                                                            *
***************************************************************************/ 
