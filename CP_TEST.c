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
#include "keypad.h"
#include "NOKIA_5100.h"
#include "screen.h"
//USART include file
#include "usart_ATmega1284.h"
//---------------------------
#define SET_BIT(p,i) ((p) |= (1 << (i)))
#define CLR_BIT(p,i) ((p) &= ~(1 << (i)))
#define GET_BIT(p,i) ((p) & (1 << (i)))

unsigned char Input_Data;
enum Keypad_Input_State {KI_INIT, GET_INPUT} KI_state;

void KI_Init()
{
	DDRB = 0xF0; PORTB = 0x0F;              //Enable the keypad
	KI_state = KI_INIT;
}

void KI_Tick()
{
	static unsigned char press;
	static unsigned char pressed;
	// Actions
	switch(KI_state)
	{
		case KI_INIT:
			press = 0x00;
			pressed = 0x00;
			Input_Data = 0x00;
			break;
		case GET_INPUT:
			press = GetKeypadKey();
			if (pressed != press)
			{
				pressed = press;
				if (press == 'A')			//A
					Input_Data = 0x0A;
				else if (press == 'B')		//B
					Input_Data = 0x0B;
				else if (press == 'C')		//C
					Input_Data = 0x0C;
				else if (press == 'D')		//D
					Input_Data = 0x0D;
				else if (press == '1')		//1
					Input_Data = 0x01;
				else if (press == '2')		//2
					Input_Data = 0x02;
				else if (press == '3')		//3
					Input_Data = 0x03;
				else if (press == '4')		//4
					Input_Data = 0x04;
				else if (press == '5')		//5
					Input_Data = 0x05;
				else if (press == '6')		//6
					Input_Data = 0x06;
				else if (press == '7')		//7
					Input_Data = 0x07;
				else if (press == '8')		//8
					Input_Data = 0x08;
				else if (press == '9')		//9
					Input_Data = 0x09;
				else
					Input_Data = press;
			}
			break;

		default:

			break;
	}
	// Transitions
	switch(KI_state)
	{
		case KI_INIT:
			KI_state = GET_INPUT;
			break;

		case GET_INPUT:

			break;

		default:
			KI_state = KI_INIT;
			break;
	}
}
// Bluetooth
enum Bluetooth_USART_SENDER_State {BUS_INIT, BUS_SENDER, BUS_ISSEND} BUS_state;

void BUS_Init()
{
	DDRD = 0x00; PORTD = 0xFF;
	// Initializes the desired USART.
	initUSART(0);
	BUS_state = BUS_INIT;
}

void BUS_Tick()
{
	static unsigned char temp_D;
	// Action
	switch(BUS_state)
	{
		case BUS_INIT:
			temp_D = 0x00;
			USART_Flush(0);
			break;

		case BUS_SENDER:

		break;

		case BUS_ISSEND:

		break;

		default:

		break;
	}
	// Transitions
	switch(BUS_state)
	{
		case BUS_INIT:
			BUS_state = BUS_SENDER;
			break;

		case BUS_SENDER:
			// PD3 is connected to the STATE of the bluetooth module
			// If bluetooth is connected, STATE is low.
			// If bluetooth is not connected, STATE is high.
			temp_D = (~PIND);
			if ((temp_D & 0x04) == 0x00 && USART_IsSendReady(0))
			{
				USART_Send(Input_Data, 0);
				BUS_state = BUS_ISSEND;
			}
			break;

		case BUS_ISSEND:
			if (USART_HasTransmitted(0))
			{
				Input_Data = 0x00;
				BUS_state = BUS_SENDER;
			}
			break;

		default:

		break;
	}
}

void CP_TEST_Task()
{
	KI_Init();
	BUS_Init();
	for (;;)
	{
		KI_Tick();
		BUS_Tick();
		vTaskDelay(100);
	}
}

void CP_TEST_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(CP_TEST_Task, (signed portCHAR *)"CP_TEST_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}


int main(void)
{
	
	//Start Tasks
	CP_TEST_Pulse(1);
	//RunSchedular
	vTaskStartScheduler();

	return 0;
}