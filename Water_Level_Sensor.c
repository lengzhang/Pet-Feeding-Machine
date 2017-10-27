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

enum Water_Sensor_State {WS_INIT, GET_LEVEL} WS_state;

void WS_Init(){
	WS_state = WS_INIT;
	adc_init();
}

void WS_Tick(){
	static uint16_t adc;
	static unsigned char* string;
	//Actions
	switch(WS_state){
		case WS_INIT:
			string = "                                ";
		break;
		case GET_LEVEL:
			adc = ReadADC3(0);
			string = "                                ";
			int i = 4;
			char j = 12345;
			int temp_one = 10;
			char port_str[6] = { 0 };
			uint16_t port = 9000;
			sprintf(port_str, "%d", (int) adc);
			LCD_DisplayString(1, port_str);
			
		break;
		default:
		
		break;
	}
	//Transitions
	switch(WS_state){
		case WS_INIT:
			WS_state = GET_LEVEL;
		break;
		case GET_LEVEL:
		
		break;
		default:
			WS_state = WS_INIT;
		break;
	}
}

void WSSecTask()
{
	WS_Init();
	for(;;)
	{
		WS_Tick();
		vTaskDelay(100);
	}
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(WSSecTask, (signed portCHAR *)"WSSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;

	DDRC = 0xFF; PORTC = 0x00;				// LCD data lines
	DDRD = 0xFF; PORTD = 0x00; 				// LCD control lines
	// Initializes the LCD display
	LCD_init();
	LCD_ClearScreen();
	LCD_DisplayString(1, "Hi");
	//Start Tasks
	StartSecPulse(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}