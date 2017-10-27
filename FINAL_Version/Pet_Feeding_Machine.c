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
#include "usart_ATmega1284.h"
//---------------------------
#define SET_BIT(p,i) ((p) |= (1 << (i)))
#define CLR_BIT(p,i) ((p) &= ~(1 << (i)))
#define GET_BIT(p,i) ((p) & (1 << (i)))

//----------USART Receiver----------
unsigned char command;
enum USART_RECEIVER_State {UR_INIT, UR_RECIVER} UR_state;

void UR_Init()
{
	UR_state = UR_INIT;
	// Initializes the desired USART.
	initUSART(0);
}

void UR_Tick()
{
	// Action
	switch(UR_state)
	{
		case UR_INIT:
			command = 0x00;
			USART_Flush(0);
			break;

		case UR_RECIVER:
			if (USART_HasReceived(0))
			{
				command = USART_Receive(0);
			}
			break;

		default:

			break;
	}
	// Transitions
	switch(UR_state)
	{
		case UR_INIT:
			UR_state = UR_RECIVER;
			break;

		case UR_RECIVER:

			break;

		default:
			UR_state = UR_INIT;
			break;
	}
}

//----------Tasks----------
void USART_RECEIVER_Task()
{
	UR_Init();
	for (;;)
	{
		UR_Tick();
		vTaskDelay(100);
	}
}

//----------System Time Pulses----------
void USART_RECEIVER_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(USART_RECEIVER_Task, (signed portCHAR *)"USART_RECEIVER_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

//----------Water Pump Control----------
enum Water_Pump_Control_State {WPC_INIT, WPC_CONTROL} WPC_state;

void Water_Pump_Control_Init()
{
	DDRB = 0xFF; PORTB = 0x00;
	WPC_state = WPC_INIT;
}

void Water_Pump_Control_Tick()
{
	// Actions
	switch(WPC_state)
	{
		case WPC_INIT:

			break;

		case WPC_CONTROL:
			if (command & 0x01)
			{
				PORTB = 0x01;
			}
			else
			{
				PORTB = 0x00;
			}
			break;

		default:

			break;
	}
	// Transitions
	switch(WPC_state)
	{
		case WPC_INIT:
			WPC_state = WPC_CONTROL;
			break;

		case WPC_CONTROL:
			WPC_state = WPC_CONTROL;
			break;

		default:
			WPC_state = WPC_INIT;
			break;
	}
}

//----------Water Pump Control Tasks----------
void Water_Pump_Control_Task()
{
	Water_Pump_Control_Init();
	for (;;)
	{
		Water_Pump_Control_Tick();
		vTaskDelay(100);
	}
}

//----------Water Pump Control Pulses----------
void Water_Pump_Control_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Water_Pump_Control_Task, (signed portCHAR *)"Water_Pump_Control_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}


//----------Drawer Control----------
unsigned char motor_steps [2][8] =
{
	{0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01},
	{0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09},
};

enum Drawer_Control_State {DC_INIT, DC_CONTROL, DC_RUN} DC_state;

void Drawer_Control_Init()
{
	DDRC = 0xFF; PORTC = 0x00;
	DC_state = DC_INIT;
}

void Drawer_Control_Tick()
{
	static unsigned char motor_index;
	static unsigned char drawer_status;
	// Actions
	switch(DC_state)
	{
		case DC_INIT:
			motor_index = 0x00;
			break;

		case DC_CONTROL:
			if (command & 0x04)
			{
				drawer_status = 0x01;
			}
			else
			{
				drawer_status = 0x00;
			}
			break;

		case DC_RUN:
			PORTC = motor_steps[drawer_status][motor_index];
			break;

		default:

			break;
	}
	// Transitions
	switch(DC_state)
	{
		case DC_INIT:
			DC_state = DC_CONTROL;
			break;

		case DC_CONTROL:
			if (command & 0x02)
			{
				motor_index = 0x00;
				DC_state = DC_RUN;
			}
			else
			{
				DC_state = DC_CONTROL;
			}
			break;

		case DC_RUN:
			motor_index++;
			if (motor_index > 0x07)
			{
				DC_state = DC_CONTROL;
			}
			break;

		default:
			DC_state = DC_INIT;
			break;
	}
}

//----------Drawer Control Tasks----------
void Drawer_Control_Task()
{
	Drawer_Control_Init();
	for (;;)
	{
		Drawer_Control_Tick();
		vTaskDelay(10);
	}
}

//----------Water Pump Control Pulses----------
void Drawer_Control_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Drawer_Control_Task, (signed portCHAR *)"Drawer_Control_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

//----------Main----------
int main(void)
{
	//Start Tasks
	USART_RECEIVER_Pulse(2);
	Water_Pump_Control_Pulse(1);
	Drawer_Control_Pulse(1);

	vTaskStartScheduler();
	
	return 0;
}