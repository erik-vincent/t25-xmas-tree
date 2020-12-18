/*
 * t84_xmas.c
 *
 * Created: 12/12/2020 3:20:20 PM
 * Author : evincent
 */

/* ATMEL ATTINY84A
                        +-\/-+
                  VCC  1|    |14 GND
             XTL1 PB0  2|    |13 PA0 ADC0 
             XTL2 PB1  3|    |12 PA1 ADC1
             RSET PB3  4|    |11 PA2 ADC2
        OC0A INT0 PB2  5|    |10 PA3 ADC3
        OC0B ADC7 PA7  6|    |9  PA4 ADC4 SCK
   MOSI OC1A ADC6 PA6  7|    |8  PA5 ADC5 MISO OC1B
		                +----+
						
	PB0 is driving LED1
	PB1 is driving LED2
	PB2 is pushing a PWM to LED3
	PA0 is driving LED8
	PA1 is driving LED7
	PA2 is driving LED6
	PA3 is driving LED5
	PA7 is pushing a PWM to LED4
	
	PA4 is not used
	PA5 is not used
	PA6 is not used

8MHz internal RC oscillator, with Divide Clock by 8 internally enabled (1MHz):
avrdude -F -c usbtiny -p t84 -U lfuse:w:0x62:m
*/

#define F_CPU 1000000UL	/* 8 MHz Clock */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <avr/interrupt.h>

#define fadeTablesize 16    // index size of the fadeTable array

volatile bool f_wdt = 1;
int tick_counter = 0;
int fadeTable[] = {255,217,185,158,134,115,98,71,60,51,44,37,32,27,20,5};

// Function prototypes
void analogWrite(uint8_t pin, int val);
void setup_watchdog(int ii);
void flashLed (int pattern);
void pattern_1(void);
void pattern_2(void);
void pattern_3(void);
void pattern_4(void);

int main(void)
{
	DDRB |= (1 << PORTB0);	// set PB0 to output mode
	DDRB |= (1 << PORTB1);	// set PB1 to output mode
	DDRB |= (1 << PORTB2);	// set PB2 to output mode
	DDRA |= (1 << PORTA0);	// set PA0 to output mode
	DDRA |= (1 << PORTA1);	// set PA1 to output mode
	DDRA |= (1 << PORTA2);	// set PA2 to output mode
	DDRA |= (1 << PORTA3);	// set PA3 to output mode
	DDRA |= (1 << PORTA7);	// set PA7 to output mode
	
	DDRA &= ~(1 << PORTA4);	// set PA4 to input mode so we can pull-up internal resistor
	DDRA &= ~(1 << PORTA5);	// set PA5 to input mode so we can pull-up internal resistor
	DDRA &= ~(1 << PORTA6);	// set PA6 to input mode so we can pull-up internal resistor
	
	PORTA |= (1 << 4);		// Set port PA4 to high to enable internal pull-up
	PORTA |= (1 << 5);		// Set port PA5 to high to enable internal pull-up
	PORTA |= (1 << 6);		// Set port PA6 to high to enable internal pull-up
	
	// Setup for OC0A and OC0B
	TCCR0A |= (1 << WGM00);	// put timer 0 in 8-bit fast pwm
	TCCR0A |= (1 << WGM01);
	TCCR0B |= (1 << CS00);	// setup to prescaler 64
	TCCR0B |= (1 << CS01);
	
	setup_watchdog(7);          // approximately 2 seconds sleep
	
	while (1) {
		
		// if watchdog flag has been set, it means the watchdog timeout has occurred
		if (f_wdt == 1) {
			// so, now that the timeout has occurred, we need to reset the flag
			f_wdt = 0;

			flashLed (tick_counter);

			// increment the tick counter so we don't do 2 of the same LED show in a row
			tick_counter++;

			// if we've gotten to the end of our LED shows...
			if(tick_counter > 3) {
				// start back over again
				tick_counter = 0;
			}

			// and after all of that, go to sleep
			setup_watchdog(7);						// setup watchdog timer to delay amount
			ADCSRA &= ~(1 << ADEN);					// switch Analog to Digitalconverter OFF
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);	// sleep mode is set here
			sleep_enable();							// sleep mode is enabled
			sleep_mode();							// System sleeps here
			sleep_disable();						// System continues execution here when watchdog timed out
			ADCSRA |= (1 << ADEN);					// switch Analog to Digitalconverter ON
		}
	}
}

/*
 * analogWrite
 *
 * function to do an analog write to PWM pin
 * 
 */ 
void analogWrite(uint8_t pin, int val) {
	
	if(pin == 5) {
		DDRB |= (1 << PORTB2); // Set PB2 to output/high
		
		if (val == 0) {
			PORTB &= ~(1 << 2);			// Set port PB2 to low
			DDRB &= ~(1 << PORTB2);		// Set PB2 to input/low
			TCCR0A &= ~(1 << COM0A1);	// disable COM0A1 on TCCR0A
			OCR0A = 0;					// turn off OCR0A
		} else {
			TCCR0A |= (1 << COM0A1);	// enable COM0A1 on TCCR0A
			OCR0A = val;				// set pwm duty
		}
	} else if (pin == 6) {
		DDRA |= (1 << PORTA7); // Set PA7 to output/high
		
		if (val == 0) {
			PORTA &= ~(1 << 7);			// Set port PA7 to low
			DDRA &= ~(1 << PORTA7);		// Set PA7 to input/low
			TCCR0A &= ~(1 << COM0B1);	// disable COM0B1 on TCCR0A
			OCR0B = 0;					// turn off OCR0B
		} else {
			TCCR0A |= (1 << COM0B1);	// enable COM0B1 on TCCR0A
			OCR0B = val;				// set pwm duty
		}
	}
}

void setup_watchdog(int ii) {
	// integer values of 4-bit WDP[3:0]
	// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
	// 6=1 sec,7=2 sec, 8=4 sec, 9=8 sec
	uint8_t bb;
	
	// Disable all interrupts
	cli();

	// if watchdog prescale select is greater than 9, which doesn't exist...
	if (ii > 9 ) {
		// then hard set to WDP[3:0] of 9, or 8 second time-out
		ii = 9;
	}

	bb = ii & 7;

	// if watchdog interval is greater than 7, so greater than 1 second...
	if (ii > 7) {
		// then shift bit 5 to a 1 to bb.
		bb |= (1 << 5);
	}

	bb |= (1 << WDCE);

	// Set WatchDog Reset Flag bit to 0 on MCU Status Register
	MCUSR &= ~(1 << WDRF);
	
	// In the Watchdog Timer Control Register, start timed sequence
	// WDTCR on tinyx5, WDTCSR on tinyx4
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	
	// In the Watchdog Timer Control Register, set new watchdog timeout value
	// WDTCR on tinyx5, WDTCSR on tinyx4
	// WDIE on tinyx4 and tinyx5, but on tiny13, WDTIE
	WDTCSR = bb;
	WDTCSR |= _BV(WDIE);
	
	// Enable all interrupts.
	sei();
}

// Interrupt Service Routine for the Watchdog Interrupt Service. Do this, when watchdog has timed out
// WDT_vect on tinyx5, WATCHDOG_vect on tinyx4
ISR(WATCHDOG_vect) {
	f_wdt = 1;  // set global flag
}

// flash the LEDs based off a pre-defined pattern set
void flashLed (int pattern) {
	DDRB |= (1 << PORTB0);	// set PB0 to output mode
	DDRB |= (1 << PORTB1);	// set PB1 to output mode
	DDRA |= (1 << PORTA0);	// set PA0 to output mode
	DDRA |= (1 << PORTA1);	// set PA1 to output mode
	DDRA |= (1 << PORTA2);	// set PA2 to output mode
	DDRA |= (1 << PORTA3);	// set PA3 to output mode
	
	// we have 4 individual patterns
	switch (pattern) {
		
		// if tick_counter is 0, do pattern 1
		case 0:
			pattern_1();
		break;
		
		// if tick_counter is 1, do pattern 2
		case 1:
			pattern_2();
		break;

		// if tick_counter is 2, do pattern 3
		case 2:
			pattern_3();
		break;
		
		// if tick_counter is 3, do pattern 4
		case 3:
			pattern_4();
		break;
	}
	
	DDRB &= ~(1 << PORTB0);	// set all used port to input to save power
	DDRB &= ~(1 << PORTB1);	// set all used port to input to save power
	DDRB &= ~(1 << PORTB2);	// set all used port to input to save power
	DDRA &= ~(1 << PORTA0);	// set all used port to input to save power
	DDRA &= ~(1 << PORTA1);	// set all used port to input to save power
	DDRA &= ~(1 << PORTA2);	// set all used port to input to save power
	DDRA &= ~(1 << PORTA3);	// set all used port to input to save power
	DDRA &= ~(1 << PORTA7);	// set all used port to input to save power
}

void pattern_1(void) {

	// lets loop from the back of the fade array to the front
	for(int i = fadeTablesize; i >= 0; i--) {
		
		// write the PWM value to pin 5 (PB2)
		analogWrite(5, fadeTable[i]);
		
		// wait 30 milliseconds
		_delay_ms(30);
	}
	
	// turn PWM to full-on on pin 5 (PB2)
	analogWrite(5,255);
	
	// wait 100 milliseconds
	_delay_ms(100);
	
	// lets loop from the front of the fade array to the back
	for(int i = 0; i < fadeTablesize; i++) {
		
		// write the PWM value to pin 5 (PB2)
		analogWrite(5, fadeTable[i]);
		
		// wait 30 milliseconds
		_delay_ms(30);
	}
	
	// turn off pin 5 (PB2)
	analogWrite(5, 0);
	
	// lets loop from the back of the fade array to the front
	for(int i = fadeTablesize; i >= 0; i--) {
		
		// write the PWM value to pin 6 (PA7)
		analogWrite(6, fadeTable[i]);
		
		// wait 30 milliseconds
		_delay_ms(30);
	}
	
	// turn PWM to full-on on pin 6 (PA7)
	analogWrite(6,255);
	
	// wait 100 milliseconds
	_delay_ms(100);
	
	// lets loop from the front of the fade array to the back
	for(int i = 0; i < fadeTablesize; i++) {
		
		// write the PWM value to pin 6 (PA7)
		analogWrite(6, fadeTable[i]);
		
		// wait 30 milliseconds
		_delay_ms(30);
	}
	
	// turn off pin 6 (PA7)
	analogWrite(6, 0);
	
	// turn on pins PB0 and PA0 (top of the tree)
	PORTB |= (1 << 0);
	PORTA |= (1 << 0);
	_delay_ms(500);		// wait 500 milliseconds
	
	PORTB |= (1 << 1);	// turn on PB1
	_delay_ms(500);		// wait 500 milliseconds
	PORTB &= ~(1 << 1);	// turn off PB1
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 1);	// turn on PA1
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 1);	// turn off PA1
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 2);	// turn on PA2
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 2);	// turn off PA2
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 3);	// turn on PA3
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 3);	// turn off PA3
	_delay_ms(30);		// wait 30 milliseconds
	
	// turn off pins PB0 and PA0 (top of the tree)
	PORTA &= ~(1 << 0);
	PORTB &= ~(1 << 0);
}

void pattern_2(void) {
	DDRB |= (1 << PORTB2);	// set PB2 to output mode
	DDRA |= (1 << PORTA7);	// set PA7 to output mode
	
	PORTB |= (1 << 0);	// turn on PB0
	_delay_ms(500);		// wait 500 milliseconds
	PORTB &= ~(1 << 0);	// turn off PB0
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTB |= (1 << 1);	// turn on PB1
	_delay_ms(500);		// wait 500 milliseconds
	PORTB &= ~(1 << 1);	// turn off PB1
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTB |= (1 << 2);	// turn on PB2
	_delay_ms(500);		// wait 500 milliseconds
	PORTB &= ~(1 << 2);	// turn off PB2
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 0);	// turn on PA0
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 0);	// turn off PA0
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 1);	// turn on PA1
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 1);	// turn off PA1
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 2);	// turn on PA2
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 2);	// turn off PA2
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 3);	// turn on PA3
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 3);	// turn off PA3
	_delay_ms(30);		// wait 30 milliseconds
	
	PORTA |= (1 << 7);	// turn on PA7
	_delay_ms(500);		// wait 500 milliseconds
	PORTA &= ~(1 << 7);	// turn off PA7
	_delay_ms(30);		// wait 30 milliseconds
}

void pattern_3(void) {
	for(int i = 0; i < 10; i++) {
		// turn off pin PB0 but turn on PA0 (top of the tree)
		PORTB &= ~(1 << 0);
		PORTA |= (1 << 0);
		_delay_ms(100);	// wait 100 milliseconds
		
		// turn off pin PA0 but turn on PB0 (top of the tree)
		PORTA &= ~(1 << 0);
		PORTB |= (1 << 0);
		_delay_ms(100);	// wait 100 milliseconds
	}
	
	// turn off pins PB0 and PA0 (top of the tree)
	PORTA &= ~(1 << 0);
	PORTB &= ~(1 << 0);
}

void pattern_4(void) {
	DDRB |= (1 << PORTB2);	// set PB2 to output mode
	DDRA |= (1 << PORTA7);	// set PA7 to output mode
	
	// Turn on all LEDs
	PORTB |= (1 << 0);
	PORTB |= (1 << 1);
	PORTB |= (1 << 2);
	PORTA |= (1 << 0);
	PORTA |= (1 << 1);
	PORTA |= (1 << 2);
	PORTA |= (1 << 3);
	PORTA |= (1 << 7);
	
	// wait 2 seconds
	_delay_ms(2000);
	
	// Turn off all LEDs
	PORTB &= ~(1 << 0);
	PORTB &= ~(1 << 1);
	PORTB &= ~(1 << 2);
	PORTA &= ~(1 << 0);
	PORTA &= ~(1 << 1);
	PORTA &= ~(1 << 2);
	PORTA &= ~(1 << 3);
	PORTA &= ~(1 << 7);
}