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

// Power Control
void Machine_Power_Init()
{
	DDRA = 0x00; PORTA = 0xFF;
}
bool Check_Mahince_Power()
{
	unsigned char temp_machine_power = (~PINA);
	return temp_machine_power & 0x01;
}

unsigned char Screen_Update;
// System Time
unsigned char hours;
unsigned char mins;
unsigned char seconds;

enum System_Time_State {ST_INIT, TIME_RUN} ST_state;

void ST_Init()
{
	ST_state = ST_INIT;
}

void ST_Tick()
{
	static unsigned char counter;
	// Actions
	switch(ST_state)
	{
		case ST_INIT:
			hours = 0x00;
			mins = 0x00;
			seconds = 0x00;
			counter = 0x09;
			Screen_Update = 0x01;
			break;

		case TIME_RUN:
			if (counter)
			{
				counter--;
			}
			//	counter == 0x00
			else
			{
				counter = 0x09;
				seconds++;
				if (seconds == 0x3C)	// 0x3c = 60
				{
					seconds = 0x00;
					mins++;
					Screen_Update = 0x01;
				}
				if (mins == 0x3C)
				{
					mins = 0x00;
					hours++;
				}
				if (hours == 0x18)		// 0x18 = 24
				{
					hours = 0x00;
				}
			}
			break;

		default:

			break;
	}
	// Transitions
	switch(ST_state)
	{
		case ST_INIT:
			ST_state = TIME_RUN;
			break;

		case TIME_RUN:

			break;

		default:
			ST_state = ST_INIT;
			break;
	}
}

// Schedule
unsigned char command;
// 0x00	-	Nothing
// 0x01	-	Water Pump Run
// 0x02	-	Food Run
// 0x04	-
// 0x08	-
// 0x10	-
// 0x20	-
// 0x40	-
// 0x80	-
typedef struct
{
	unsigned char ENABLE;
	unsigned char Executed;
	unsigned char HOUR;
	unsigned char MIN;
} MySchedule;

MySchedule my_schedule[3];

enum Machine_Schedule_State {MS_INIT, MS_CHECK, MS_CLEAR} MS_state;

void MS_Init()
{
	MS_state = MS_INIT;
}

void MS_Tick()
{
	// Actions
	switch(MS_state)
	{
		case MS_INIT:
			command = 0x00;
			for (int i = 0; i < 3; i++)
			{
				my_schedule[i].ENABLE = 0x00; my_schedule[i].Executed = 0x00; my_schedule[i].HOUR = 0x00; my_schedule[i].MIN = 0x00;
			}
			break;

			case MS_CHECK:
			for (int i = 0; i < 3; i++)
			{
				if (my_schedule[i].ENABLE && !my_schedule[i].Executed && my_schedule[i].HOUR == hours && my_schedule[i].MIN == mins)
				{
					my_schedule[i].Executed = 0x01;

					command |= 0x03;
				}
			}
			break;

		case MS_CLEAR:
			for (int i = 0; i < 3; i++)
			{
				if (my_schedule[i].Executed && my_schedule[i].HOUR <= hours && my_schedule[i].MIN < mins)
				{
					my_schedule[i].Executed = 0x00;
				}
			}
			break;

		default:

			break;
	}

	// Transitions
	switch(MS_state)
	{
		case MS_INIT:
			MS_state = MS_CHECK;
			break;

		case MS_CHECK:
			MS_state = MS_CLEAR;
			break;

		case MS_CLEAR:
			MS_state = MS_CHECK;
			break;

		default:

			break;
	}
}

// Bluetooth
unsigned char connected;
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
	static unsigned char counter;
	static unsigned char temp_D;
	// Action
	switch(BUS_state)
	{
		case BUS_INIT:
			connected = 0x00;
			counter = 0x00;
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
				USART_Send(command, 0);
				if (connected == 0x00)
				{
					Screen_Update = 0x01;
					connected = 0x01;
					counter = 0x00;
				}
				BUS_state = BUS_ISSEND;
			}
			else if (counter == 4)
			{
				if (connected == 0x01)
				{
					Screen_Update = 0x01;
				}
				connected = 0x00;
			}
			else if (counter < 4)
			{
				counter++;
			}
			break;

		case BUS_ISSEND:
			if (USART_HasTransmitted(0))
			{
				command = 0x00;
				BUS_state = BUS_SENDER;
			}
			break;
		default:

			break;
	}
}

unsigned char Input_Data;
unsigned char Key_Pad_Enable;
enum Keypad_Input_State {KI_INIT, GET_INPUT} KI_state;

void KI_Init()
{
	KI_state = KI_INIT;
	DDRB = 0xF0; PORTB = 0x0F;              //Enable the keypad
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
			Key_Pad_Enable = 0x00;
			break;

		case GET_INPUT:
			if (Key_Pad_Enable)
			{
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
				}
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

unsigned char *display_string[504];

void Set_Display_Base()
{
	// Bluetooth Connection
	if (connected)
	{
		display_string[0] = 0x24;
		display_string[1] = 0x18;
		display_string[2] = 0xFF;
		display_string[3] = 0x5A;
		display_string[4] = 0x24;
	}
	else
	{
		display_string[0] = 0x42;
		display_string[1] = 0x24;
		display_string[2] = 0x18;
		display_string[3] = 0x24;
		display_string[4] = 0x42;
	}
	// Display System Time
	for (int i = 0; i < 5; i++)
	{
		display_string[53+i] = FONT_5x7[((hours / 10) % 10) + 16][i];
		display_string[59+i] = FONT_5x7[(hours % 10) + 16][i];

		display_string[65+i] = FONT_5x7[26][i];					// :

		display_string[71+i] = FONT_5x7[((mins / 10) % 10) + 16][i];
		display_string[77+i] = FONT_5x7[(mins % 10) + 16][i];
	}
}

void Set_Fram(unsigned char option)
{
	for (int i = 0; i < 504; i++)
	{
		display_string[i] = Frams[option][i];
	}
}

void Set_Menu_Options(unsigned char option)
{
	for (int i = 0; i < 84; i++)
	{
		display_string[252+i] = Menu_Options[option][i];
	}
}

void Set_SetTime_Options(unsigned char option, unsigned char HOURS, unsigned char MINS)
{
	for (int i = 0; i < 5; i++)
	{
		display_string[276+i] = FONT_5x7[((HOURS / 10) % 10) + 16][i];
		display_string[282+i] = FONT_5x7[(HOURS % 10) + 16][i];

		display_string[294+i] = FONT_5x7[26][i];					// :

		display_string[306+i] = FONT_5x7[((MINS / 10) % 10) + 16][i];
		display_string[312+i] = FONT_5x7[(MINS % 10) + 16][i];
	}
	if (option == 0)
	{
		display_string[194] = 0x20;	display_string[224] = 0x00;
		display_string[195] = 0x30;	display_string[225] = 0x00;
		display_string[196] = 0x38;	display_string[226] = 0x00;
		display_string[197] = 0x3C;	display_string[227] = 0x00;
		display_string[198] = 0x38;	display_string[228] = 0x00;
		display_string[199] = 0x30;	display_string[229] = 0x00;
		display_string[200] = 0x20;	display_string[230] = 0x00;

		display_string[362] = 0x02;	display_string[392] = 0x00;
		display_string[363] = 0x06;	display_string[393] = 0x00;
		display_string[364] = 0x0E;	display_string[394] = 0x00;
		display_string[365] = 0x1E;	display_string[395] = 0x00;
		display_string[366] = 0x0E;	display_string[396] = 0x00;
		display_string[367] = 0x06;	display_string[397] = 0x00;
		display_string[368] = 0x02;	display_string[398] = 0x00;
	}
	else if (option == 1)
	{
		display_string[194] = 0x00;	display_string[224] = 0x20;
		display_string[195] = 0x00;	display_string[225] = 0x30;
		display_string[196] = 0x00;	display_string[226] = 0x38;
		display_string[197] = 0x00;	display_string[227] = 0x3C;
		display_string[198] = 0x00;	display_string[228] = 0x38;
		display_string[199] = 0x00;	display_string[229] = 0x30;
		display_string[200] = 0x00;	display_string[230] = 0x20;

		display_string[362] = 0x00;	display_string[392] = 0x02;
		display_string[363] = 0x00;	display_string[393] = 0x06;
		display_string[364] = 0x00;	display_string[394] = 0x0E;
		display_string[365] = 0x00;	display_string[395] = 0x1E;
		display_string[366] = 0x00;	display_string[396] = 0x0E;
		display_string[367] = 0x00;	display_string[397] = 0x06;
		display_string[368] = 0x00;	display_string[398] = 0x02;
	}
}

void Set_Schedule_Options(unsigned char option, unsigned char index)
{
	for (int i = 0; i < 84; i++)
	{
		display_string[252+i] = Schedule_Options[option][i];
	}
	/*
	for (int i = 0; i < 84; i++)
	{
		display_string[420+i] = Schedule_Hint[index+i];
	}
	*/
}

void Set_Dosage_Option(unsigned char option)
{
	for (int i = 0; i < 84; i++)
	{
		display_string[252+i] = Dosage_Options[option][i];
	}
}

void Set_Hit_Screen(const unsigned char Hint_Array[], unsigned char index)
{
	for (int i = 0; i < 84; i++)
	{
		display_string[420+i] = Hint_Array[index+i];
	}
}

void Print_Display_String()
{
	// Display String
	LCD_GoTo_xy(0,0);
	for (int i=0; i<504; i++)
	{
		LCD_Col(display_string[i]);
	}
}

enum SCREEN_State {SCREEN_INIT, SCREEN_ON, SCREEN_OFF, SCREEN_WELCOME, SCREEN_MENU, SCREEN_SET_TIME, SCREEN_SCHEDULE, SCREEN_SET_SCHEDULE, SCREEN_DOSAGE} S_state;

void S_Init()
{
	DDRC = 0xFF; PORTC = 0x00;
}

void S_Tick()
{
	static unsigned char counter;
	static unsigned char Machine_Power;
	static unsigned char index;
	static unsigned char index_time;
	static unsigned char scroll_counter;
	static unsigned char scroll_direction;

	static unsigned char HOURS;
	static unsigned char MINS;
	static unsigned char ENABLE;

	static unsigned char dosage;
	// Actions
	switch(S_state)
	{
		//----------------------------------------
		case SCREEN_INIT:
			for (int i=0; i<504; i++)
			{
				display_string[i] = BlankScreen[i];
			}
			counter = 0;
			dosage = 0x01;
			command = 0x10;
			break;
		//----------------------------------------
		case SCREEN_ON:
			//Enable power of the LCD
			SET_BIT(PORTC, 2);
			//Initializing LCD
			// define the 5 LCD Data pins: SCE, RST, DC, DATA, CLK
			LCD_Init(&PORTC, PC7, &PORTC, PC6, &PORTC, PC5, &PORTC, PC4, &PORTC, PC3);
			break;
		//----------------------------------------
		case SCREEN_OFF:
		
			break;
		//----------------------------------------
		case SCREEN_WELCOME:
			if (counter == 0)
			{
				Print_Picture_On_LCD(WelcomeScreen);
			}
			break;
		//----------------------------------------
		case SCREEN_MENU:
			// Display Screen
			if (Screen_Update)
			{
				Set_Display_Base();
				Set_Menu_Options(index);
				Print_Display_String();
				Screen_Update = 0x00;
			}
			break;
		//----------------------------------------
		case SCREEN_SET_TIME:
			if (Screen_Update)
			{
				Set_Display_Base();
				Set_SetTime_Options(index_time, HOURS, MINS);
				Print_Display_String();
				Screen_Update = 0x00;
			}
			break;
		
		//----------------------------------------
		case SCREEN_SCHEDULE:
			if (Screen_Update)
			{
				Set_Display_Base();
				Set_Schedule_Options(index, scroll_counter);
				Set_Hit_Screen(Schedule_Hint, scroll_counter);
				Print_Display_String();
				Screen_Update = 0x00;
			}
			counter++;
			if (counter > 1)
			{
				counter = 0x00;
				if (scroll_direction)
				{
					scroll_counter--;
				}
				else
				{
					scroll_counter++;
				}
				
				if (scroll_counter == 0x00)
				{
					scroll_direction = 0x00;
				}
				else if (scroll_counter == 0x18)
				{
					scroll_direction = 0x01;
				}
				Screen_Update = 0x01;
			}
			break;
		//----------------------------------------
		case SCREEN_SET_SCHEDULE:
			if (Screen_Update)
			{
				Set_Display_Base();
				if (ENABLE)
				{
					for (int i = 0; i < 252; i++)
					{
						display_string[168 + i] = 0x00;
					}
					Set_SetTime_Options(index_time, HOURS, MINS);
				}
				else
				{
					for (int i = 0; i < 252; i++)
					{
						display_string[168 + i] = Set_Off_Screen[i];
					}
				}
				Set_Hit_Screen(Set_Schedule_Hint, scroll_counter);
				Print_Display_String();
				Screen_Update = 0x00;
			}
			counter++;
			if (counter > 1)
			{
				counter = 0x00;
				if (scroll_direction)
				{
					scroll_counter--;
				}
				else
				{
					scroll_counter++;
				}
				
				if (scroll_counter == 0x00)
				{
					scroll_direction = 0x00;
				}
				else if (scroll_counter == 0x3C)
				{
					scroll_direction = 0x01;
				}
				Screen_Update = 0x01;
			}
			break;

		case SCREEN_DOSAGE:
			if (Screen_Update)
			{
				Set_Display_Base();
				Set_Dosage_Option(index);
				Set_Hit_Screen(Schedule_Hint, scroll_counter);
				Print_Display_String();
				Screen_Update = 0x00;
			}
			break;

		default:

			break;
	}
	// Transitions
	switch(S_state)
	{
		//----------------------------------------
		case SCREEN_INIT:
			if (Check_Mahince_Power())
			{
				Machine_Power = 0x01;
				S_state = SCREEN_ON;
			}
			else
			{
				Machine_Power = 0x00;
				S_state = SCREEN_OFF;
			}
			break;
		//----------------------------------------
		case SCREEN_ON:
			counter = 0;
			index = 0;
			S_state = SCREEN_WELCOME;
			break;
		//----------------------------------------
		case SCREEN_OFF:
			if (Check_Mahince_Power() == false && Machine_Power == 0x01)
			{
				LCD_Clear();
				CLR_BIT(PORTC, 2);
				Machine_Power = 0x00;
				Key_Pad_Enable = 0x00;
			}
			else if (Check_Mahince_Power() && Machine_Power == 0x00)
			{
				S_state = SCREEN_ON;
				Machine_Power = 0x01;
			}
			break;
		//----------------------------------------
		case SCREEN_WELCOME:
			if (counter >= 19)
			{
				Key_Pad_Enable = 0x01;
				Set_Fram(0);
				Screen_Update = 0x01;
				index = 0;
				S_state = SCREEN_MENU;
			}
			counter++;
			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			break;
		//----------------------------------------
		case SCREEN_MENU:
			// Control
			// 4 - Left
			if (Input_Data == 0x04)
			{
				if (index == 0)
				{
					index = 3;
				}
				else
				{
					index--;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}
			// 6 - Right
			else if (Input_Data == 0x06)
			{
				if (index == 3)
				{
					index = 0;
				}
				else
				{
					index++;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}

			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			// A - Enter
			// Manual
			else if (Input_Data == 0x0A && index == 0)
			{
				command |= 0x03;
				Input_Data = 0x00;
			}
			// Set Schedule
			else if (Input_Data == 0x0A && index == 1)
			{
				index = 0;
				Input_Data = 0x00;
				Set_Fram(1);
				Screen_Update = 0x01;
				counter = 0x00;
				scroll_counter = 0x00;
				scroll_direction = 0x00;
				S_state = SCREEN_SCHEDULE;
			}
			// Set Time
			else if (Input_Data == 0x0A && index == 2)
			{
				index_time = 0;
				Input_Data = 0x00;
				HOURS = hours;
				MINS = mins;
				Set_Fram(2);
				Screen_Update = 0x01;
				S_state = SCREEN_SET_TIME;
			}
			// Dosage
			else if (Input_Data == 0x0A && index == 3)
			{
				index = dosage - 1;
				Input_Data = 0x00;
				Set_Fram(3);
				Screen_Update = 0x01;
				S_state = SCREEN_DOSAGE;
			}
			break;
		//----------------------------------------
		case SCREEN_SET_TIME:
			// Control
			// 4 - Left, 6 - Right
			if (Input_Data == 0x04 || Input_Data == 0x06)
			{
				index_time = (~index_time) & 0x01;
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}
			// 2 - Increase
			else if (Input_Data == 0x02)
			{
				if (index_time)
				{
					MINS++;
					if (MINS == 0x3C)
					{
						MINS = 0x00;
					}
				}
				else
				{
					HOURS++;
					if (HOURS == 0x18)
					{
						HOURS = 0x00;
					}
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}
			// 8 - Decrease
			else if (Input_Data == 0x08)
			{
				if (index_time)
				{
					if (MINS == 0x00)
					{
						MINS = 0x3C;
					}
					MINS--;
				}
				else
				{
					if (HOURS == 0x00)
					{
						HOURS = 0x18;
					}
					HOURS--;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}

			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			else if (Input_Data == 0x0A)
			{
				Set_Fram(0);
				Screen_Update = 0x01;
				hours = HOURS;
				mins = MINS;
				seconds = 0x00;
				index = 2;
				index_time = 0;
				Input_Data = 0x00;
				S_state = SCREEN_MENU;
			}
			else if (Input_Data == 0x0B)
			{
				Set_Fram(0);
				Screen_Update = 0x01;
				index = 2;
				index_time = 0;
				Input_Data = 0x00;
				S_state = SCREEN_MENU;
			}
			break;
		//----------------------------------------
		case SCREEN_SCHEDULE:
			// Control
			// 4 - Left
			if (Input_Data == 0x04)
			{
				if (index == 0)
				{
					index = 2;
				}
				else
				{
					index--;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}
			// 6 - Right
			else if (Input_Data == 0x06)
			{
				if (index == 2)
				{
					index = 0;
				}
				else
				{
					index++;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}

			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			else if (Input_Data == 0x0A)
			{
				Set_Fram(3);
				for (int i = 24; i <= 54; i++)
				{
					display_string[84+i] = Schedule_Options[index][i];
				}

				Screen_Update = 0x01;
				index_time = 0;
				ENABLE = my_schedule[index].ENABLE;
				HOURS = my_schedule[index].HOUR;
				MINS = my_schedule[index].MIN;
				Input_Data = 0x00;
				counter = 0x00;
				scroll_counter = 0x00;
				scroll_direction = 0x00;
				S_state = SCREEN_SET_SCHEDULE;
			}
			else if (Input_Data == 0x0B)
			{
				Set_Fram(0);
				Screen_Update = 0x01;
				index = 1;
				index_time = 0;
				Input_Data = 0x00;
				S_state = SCREEN_MENU;
			}
			break;
		//----------------------------------------
		case SCREEN_SET_SCHEDULE:
			if (ENABLE)
			{
				// Control
				// 4 - Left, 6 - Right
				if (Input_Data == 0x04 || Input_Data == 0x06)
				{
					index_time = (~index_time) & 0x01;
					Screen_Update = 0x01;
					Input_Data = 0x00;
				}
				// 2 - Increase
				else if (Input_Data == 0x02)
				{
					if (index_time)
					{
						MINS++;
						if (MINS == 0x3C)
						{
							MINS = 0x00;
						}
					}
					else
					{
						HOURS++;
						if (HOURS == 0x18)
						{
							HOURS = 0x00;
						}
					}
					Screen_Update = 0x01;
					Input_Data = 0x00;
				}
				// 8 - Decrease
				else if (Input_Data == 0x08)
				{
					if (index_time)
					{
						if (MINS == 0x00)
						{
							MINS = 0x3C;
						}
						MINS--;
					}
					else
					{
						if (HOURS == 0x00)
						{
							HOURS = 0x18;
						}
						HOURS--;
					}
					Screen_Update = 0x01;
					Input_Data = 0x00;
				}
			}
			// C - On / Off
			if (Input_Data == 0x0C)
			{
				ENABLE = (~ENABLE) & 0x01;
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}

			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			else if (Input_Data == 0x0A)
			{
				Input_Data = 0x00;
				Set_Fram(1);
				Screen_Update = 0x01;
				counter = 0x00;
				scroll_counter = 0x00;
				scroll_direction = 0x00;
				S_state = SCREEN_SCHEDULE;

				my_schedule[index].ENABLE = ENABLE;
				my_schedule[index].HOUR = HOURS;
				my_schedule[index].MIN = MINS;
				index_time = 0;
			}
			else if (Input_Data == 0x0B)
			{
				Input_Data = 0x00;
				Set_Fram(1);
				Screen_Update = 0x01;
				counter = 0x00;
				scroll_counter = 0x00;
				scroll_direction = 0x00;
				S_state = SCREEN_SCHEDULE;
			}
			break;
		//----------------------------------------
		case SCREEN_DOSAGE:
			// Control
			// 4 - Left
			if (Input_Data == 0x04)
			{
				if (index == 0)
				{
					index = 4;
				}
				else
				{
					index--;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}
			// 6 - Right
			else if (Input_Data == 0x06)
			{
				if (index == 4)
				{
					index = 0;
				}
				else
				{
					index++;
				}
				Screen_Update = 0x01;
				Input_Data = 0x00;
			}

			if (!Check_Mahince_Power())
			{
				S_state = SCREEN_OFF;
			}
			// A - Enter
			// Manual
			else if (Input_Data == 0x0A)
			{
				if (index == 0)
				{
					dosage = 0x01;
					command |= 0x10;
				}
				else if (index == 1)
				{
					dosage = 0x02;
					command |= 0x20;
				}
				else if (index == 2)
				{
					dosage = 0x03;
					command |= 0x30;
				}
				else if (index == 3)
				{
					dosage = 0x04;
					command |= 0x40;
				}
				else if (index == 4)
				{
					dosage = 0x05;
					command |= 0x50;
				}

				Set_Fram(0);
				Screen_Update = 0x01;
				index = 3;
				Input_Data = 0x00;
				S_state = SCREEN_MENU;
			}
			else if (Input_Data == 0x0B)
			{
				Set_Fram(0);
				Screen_Update = 0x01;
				index = 3;
				Input_Data = 0x00;
				S_state = SCREEN_MENU;
			}

			break;
		//----------------------------------------
		default:

		break;
	}
}

void System_Time_Task()
{
	ST_Init();
	for (;;)
	{
		ST_Tick();
		vTaskDelay(100);
	}
}

void Schedule_Bluetooth_Task()
{
	MS_Init();
	BUS_Init();
	for (;;)
	{
		MS_Tick();
		BUS_Tick();
		vTaskDelay(100);
	}
}

void CPSecTask()
{
	Machine_Power_Init();
	KI_Init();
	S_Init();
	for(;;)
	{
		KI_Tick();
		S_Tick();
		vTaskDelay(100);
	}
}

void SystemTimePulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(System_Time_Task, (signed portCHAR *)"System_Time_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void ScheduleBluetoothPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Schedule_Bluetooth_Task, (signed portCHAR *)"Schedule_Bluetooth_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void ControlPanelPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(CPSecTask, (signed portCHAR *)"CPSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void)
{
	
	//Start Tasks
	SystemTimePulse(3);
	ScheduleBluetoothPulse(2);
	ControlPanelPulse(1);
	//RunSchedular
	vTaskStartScheduler();

	return 0;
}