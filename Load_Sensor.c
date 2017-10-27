#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/portpins.h>
#include <avr/pgmspace.h>

//FreeRTOS include files
#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

//---------------------------
#include "bit.h"
#include "lcd.h"
//---------------------------

void adc_init()
{
	// AREF = AVcc
	ADMUX = (1<<REFS0);
	
	// ADC Enable and prescaler of 128
	// 16000000/128 = 125000
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

uint16_t ReadADC3(uint16_t ch){
	//Select ADC Channel. ch must be 0-7
	ADMUX = (ADMUX & 0xF8) | (ch & 0x1F);
	ADCSRB = (ADCSRB & 0xDF) | (ch & 0x20);
	//Start single conversion
	ADCSRA |= (1<<ADSC);
	while (ADCSRA & (1<<ADSC));
	return (ADC);
}

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	adc_init();

	DDRC = 0xFF; PORTC = 0x00;				// LCD data lines
	DDRD = 0xFF; PORTD = 0x00; 				// LCD control lines
	// Initializes the LCD display
	LCD_init();
	LCD_ClearScreen();
	LCD_DisplayString(1, "Hi");
	
	while(1)
	{
		uint16_t adc;
		CLR_BIT(PORTD, 0);
		adc = ReadADC3(0);
		SET_BIT(PORTD, 0);

		char port_str[6] = { 0 };
		sprintf(port_str, "%d", (int) adc);
		LCD_DisplayString(1, port_str);
		delay_ms(100);
	}



	return 0;
}