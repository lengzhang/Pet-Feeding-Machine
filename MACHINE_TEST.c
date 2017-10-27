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
#include "usart_ATmega1284.h"
//---------------------------
#define SET_BIT(p,i) ((p) |= (1 << (i)))
#define CLR_BIT(p,i) ((p) &= ~(1 << (i)))
#define GET_BIT(p,i) ((p) & (1 << (i)))
//----------System Time----------
unsigned char system_time [3];	//	0 - hour | 1 - min | 2 - second

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
			system_time[0] = 0x00;	//	Initialize Hour
			system_time[1] = 0x00;	//	Initialize Minute
			system_time[2] = 0x00;	//	Initialize Second

			counter = 0x09;
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
				system_time[2]++;
				if (system_time[2] == 0x3C)	// 0x3c = 60
				{
					system_time[2] = 0x00;
					system_time[1]++;
				}
				if (system_time[1] == 0x3C)
				{
					system_time[1] = 0x00;
					system_time[0]++;
				}
				if (system_time[0] == 0x18)		// 0x18 = 24
				{
					system_time[0] = 0x00;
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

//----------ADC----------
void adc_init()
{
	// AREF = AVcc
	ADMUX = (1<<REFS0);
	
	// ADC Enable and prescaler of 128
	// 16000000/128 = 125000
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

uint16_t ReadADC3(uint16_t ch){
	// Select ADC Channel. ch must be 0-7
	ADMUX = (ADMUX & 0xF8) | (ch & 0x1F);
	ADCSRB = (ADCSRB & 0xDF) | (ch & 0x20);
	// Start single conversion
	ADCSRA |= (1<<ADSC);
	while (ADCSRA & (1<<ADSC));
	return (ADC);
}
//----------ADC----------

//----------Bluetooth----------
unsigned char activate_water_pump	= 0x00;	//	0 - NULL	|	1 - Activate
unsigned char activate_drawer		= 0x00;	//	0 - NULL	|	1 - Activate

enum Bluetooth_USART_RECEIVER_State {BUR_INIT, BUR_RECIVER, PROCESSING} BUR_state;

void BUR_Init()
{
	BUR_state = BUR_INIT;
	// Initializes the desired USART.
	initUSART(0);
}

void BUR_Tick()
{
	static unsigned char command = 0x00;
	// 0x00	-	Nothing
	// 0x01	-	Activate Water Pump On / Off
	// 0x02	-	Activate Drawer In / Out
	// 0x04	-
	// 0x08	-
	// 0x10	-
	// 0x20	-
	// 0x40	-
	// 0x80	-

	// Action
	switch(BUR_state)
	{
		case BUR_INIT:
			command = 0x00;
			USART_Flush(0);
			break;
		case BUR_RECIVER:

			break;
		case PROCESSING:

			break;
		default:

			break;
	}
	// Transitions
	switch(BUR_state)
	{
		case BUR_INIT:
			BUR_state = BUR_RECIVER;
			break;

		case BUR_RECIVER:
			if (((~PIND) & 0x04) == 0x00 && USART_HasReceived(0))
			{
				command = USART_Receive(0);
				BUR_state = PROCESSING;
			}
			break;

		case PROCESSING:

			//LCD_DisplayString(1, command);
			
			// Activate Water Pump
			if ((command & 0x01) && activate_water_pump == 0x00)
			{
				activate_water_pump = 0x01;
			}
			// Activate Drawer
			if ((command & 0x02) && activate_drawer == 0x00)
			{
				activate_drawer = 0x01;
			}
			command = 0x00;
			
			BUR_state = BUR_RECIVER;
			break;
		default:

		break;
	}
}

//----------Water Sensor & Force Sensing Resistor----------
enum Water_Sensor_and_Force_Sensing_Resistor_State {SENSOR_INIT, SENSOR_GET, SENSOR_WAIT} SENSOR_state;

void SENSOR_Init(){
	SENSOR_state = SENSOR_INIT;
	adc_init();
}

void SENSOR_Tick(){
	static unsigned char *display_string;	//	For Display on LCD
	static uint16_t water_sensor_adc;
	static uint16_t force_sensing_resistor_adc;

	static unsigned char begin_time[3];
	//Actions
	switch(SENSOR_state){
		case SENSOR_INIT:
			display_string = "Water ADC:      Load ADC:       ";
			force_sensing_resistor_adc = 0;
			water_sensor_adc = 0;

			begin_time[0] = 0x00;
			begin_time[1] = 0x00;
			begin_time[2] = 0x00;
			break;

		case SENSOR_GET:
			// Get Average ADCs
			force_sensing_resistor_adc = 0;
			water_sensor_adc = 0;
			for (int i = 0; i < 32; i++)
			{
				// Get Force Sensing Resistor ADC
				force_sensing_resistor_adc += ReadADC3(0);
				// Get Water Level Sensor ADC
				water_sensor_adc += ReadADC3(1);
			}
			force_sensing_resistor_adc = (uint16_t) (force_sensing_resistor_adc / 32);
			water_sensor_adc = (uint16_t) (water_sensor_adc / 32);

			//	Check Water Sensor ADC
			if ((int)(water_sensor_adc / 10) < 70 && activate_water_pump == 0x00)
			{
				activate_water_pump = 0x01;
			}
			else if ((int)(water_sensor_adc / 10) > 80 && activate_water_pump == 0x01)
			{
				activate_water_pump = 0x00;
			}
			// Check Force Sensing Resistor ADC

			// Display to LCD
			char port_str[6] = { 0 };
			sprintf(port_str, "%d", (int) water_sensor_adc);
			for (int i = 0; i < 6; ++i)
			{
				display_string[10+i] = port_str[i];
			}
			sprintf(port_str, "%d", (int) force_sensing_resistor_adc);
			for (int i = 0; i < 6; ++i)
			{
				display_string[26+i] = port_str[i];
			}

			LCD_DisplayString(1, display_string);
			break;

		case SENSOR_WAIT:

			break;
		default:
		
			break;
	}
	//Transitions
	switch(SENSOR_state){
		case SENSOR_INIT:
			SENSOR_state = SENSOR_GET;
			break;

		case SENSOR_GET:
			begin_time[0] = system_time[0];
			begin_time[1] = system_time[1];
			begin_time[2] = system_time[2];

			SENSOR_state = SENSOR_WAIT;
			break;

		case SENSOR_WAIT:
			//	Wait for 1 second, so get ADC from sensors every 1 second.
			// 	Since if already waiting 1 second, the system time
			//	must different than the time when start to wait.
			if (begin_time[0] != system_time[0]
				|| begin_time[1] != system_time[1]
				|| begin_time[2] != system_time[2])
			{
				SENSOR_state = SENSOR_GET;
			}
			break;

		default:
			SENSOR_state = SENSOR_INIT;
			break;
	}
}
/*
//----------Water Sensor----------
enum Water_Sensor_State {WS_INIT, WS_GET, WS_WAIT} WS_state;

void WS_Init(){
	WS_state = WS_INIT;
}

void WS_Tick(){
	static uint16_t water_level_adc;
	static unsigned char counter;
	//Actions
	switch(WS_state){
		case WS_INIT:
			counter = 0;
			break;

		case WS_GET:
			water_level_adc = 0;
			for (int i = 0; i < 32; i++)
			{
				water_level_adc += ReadADC3(0);
			}
			water_level_adc = (uint16_t) (water_level_adc / 32);

			if ((int)(water_level_adc / 10) < 70)
			{
				activate_water_pump = 0x01;
			}
			else if ((int)(water_level_adc / 10) > 80)
			{
				activate_water_pump = 0x00;
			}

			char port_str[6] = { 0 };
			sprintf(port_str, "%d", (int) water_level_adc);
			for (int i = 0; i < 6; ++i)
			{
				display_string[10+i] = port_str[i];
			}
			break;

		case WS_WAIT:

			break;
		default:
		
			break;
	}
	//Transitions
	switch(WS_state){
		case WS_INIT:
			WS_state = WS_GET;
			break;

		case GET_LEVEL:
			counter = 0;
			WS_state = WS_WAIT;
			break;

		case WS_WAIT:
			if (counter > 8)
			{
				WS_state = WS_GET;
			}
			counter++;
			break;

		default:
			WS_state = WS_INIT;
			break;
	}
}
*/

//----------Water Pump----------
enum Water_Pump_State {WP_INIT, WP_RUN} WP_state;

void WP_Init(){
	WP_state = WP_INIT;
	activate_water_pump = 0x00;
}

void WP_Tick(){
	static unsigned char current_water_pump_state;
	// Action
	switch(WP_state)
	{
		case WP_INIT:
			current_water_pump_state = 0x00;
			break;

		case WP_RUN:
			if (activate_water_pump)
			{
				current_water_pump_state = (~current_water_pump_state) & 0x01;
				if (current_water_pump_state)
				{
					SET_BIT(PORTD, 3);
				}
				else
				{
					CLR_BIT(PORTD, 3);
				}
				activate_water_pump = 0x00;
			}
			break;

		default:

			break;
	}
	//Transitions
	switch(WP_state)
	{
		case WP_INIT:
			WP_state = WP_RUN;
			break;

		case WP_RUN:

			break;

		default:
			WP_state = WP_INIT;
			break;
	}
}

//----------Drawer----------
unsigned char motor_step [2][8] =
{
	{0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01},
	{0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09}
};

enum Drawer_State {D_INIT, D_WAIT, D_SET, D_MOVE} D_state;

void D_Init(){
	D_state = D_INIT;
	activate_drawer = 0x00;
}

void D_Tick()
{
	static unsigned char motor_steps [2][8] =
	{
		{0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01},
		{0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09}
	};
	static unsigned char current_drawer_state;	// 0 - in | 1 - out
	static unsigned char motor_index;
	static unsigned char current_motor_phase;
	// Action
	switch(D_state)
	{
		case D_INIT:
			
			current_drawer_state = 0x00;
			motor_index = 0x00;
			current_motor_phase = 0x00;
			break;

		case D_WAIT:

			break;

		case D_SET:
			motor_index = 0x00;
			current_motor_phase = 0x00;
			break;

		case D_MOVE:
			if (motor_index > 0x07)
			{
				motor_index = 0x00;
				current_motor_phase+=5.625;
			}

			motor_index++;
			break;

		default:

			break;
	}
	//Transitions
	switch(D_state)
	{
		case D_INIT:
			D_state = D_WAIT;
			break;

		case D_WAIT:

			break;

		case D_SET:
			D_state = D_MOVE;
			break;

		case D_MOVE:

			break;

		default:
			D_state = D_INIT;
			break;
	}
}

//----------Tasks----------
void System_Time_Task()
{
	ST_Init();
	for (;;)
	{
		ST_Tick();
		vTaskDelay(100);
	}
}

void Execute_Task()
{
	BUR_Init();
	//SENSOR_Init();
	//WP_Init();
	for(;;)
	{
		BUR_Tick();
		//SENSOR_Tick();
		//WP_Tick();
		vTaskDelay(100);
	}
}

// Pulses
void SystemTimePulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(System_Time_Task, (signed portCHAR *)"System_Time_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void ExecutePulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Execute_Task, (signed portCHAR *)"Execute_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

// Mains
int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;

	DDRB = 0xFF; PORTB = 0x00;

	DDRC = 0xFF; PORTC = 0x00;				// LCD data lines
	DDRD = 0xFF; PORTD = 0x00; 				// LCD control lines
	// Initializes the LCD display
	LCD_init();
	LCD_ClearScreen();
	LCD_DisplayString(1, "Command:");
	LCD_Cursor(33);
	// Initializes the desired USART.
	initUSART(0);


	while(1)
	{
		if (USART_HasReceived(0))
		{
			unsigned char command = USART_Receive(0);
			if (command)
			{
				for (int i = 0; i < 8; ++i)
				{
					LCD_Cursor(24-i);
					if((command) & (1 << (i)))
					{
						LCD_WriteData('1');
					}
					else
					{
						LCD_WriteData('0');
					}
				}
				LCD_Cursor(33);
				if (command & 0x01)
				{
					SET_BIT(PORTD, 3);
				}
				else
				{
					CLR_BIT(PORTD, 3);
				}
				if (command & 0x02)
				{
					SET_BIT(PORTD, 4);
				}
				else
				{
					CLR_BIT(PORTD, 4);
				}
			}
		}
		
	}

	//Start Tasks
	//SystemTimePulse(2);
	//ExecutePulse(1);
	//RunSchedular
	//vTaskStartScheduler();
	
	return 0;
}