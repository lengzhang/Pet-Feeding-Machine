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

enum Water_Pump_State {WP_INIT, Water_Out, Water_Back} WP_state;

void WP_Init(){
	WP_state = INIT;
	adc_init();
}

void WP_Tick(){
	//Actions
	switch(WP_state){
		case WP_INIT:
			break;
		case Water_Out:
			
		break;
		case Water_Back:
			
		break;
		default:
		
		break;
	}
	//Transitions
	switch(WP_state){
		case INIT:
			WP_state = Water_Out;
		break;
		case Water_Out:
			
		break;
		case Water_Back:
			
		break;
		default:
			WP_state = WP_INIT;
		break;
	}
}

void WPSecTask()
{
	WP_Init();
	for(;;)
	{
		WP_Tick();
		vTaskDelay(100);
	}
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(WPSecTask, (signed portCHAR *)"WPSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	//Start Tasks
	StartSecPulse(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}