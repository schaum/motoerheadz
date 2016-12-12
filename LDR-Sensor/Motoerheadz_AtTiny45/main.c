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
// #include <inttypes.h>

// functions for key measuring and debouncing
#include "Keys.h"

// pins
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

int main(void) {  
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
   return 0;
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