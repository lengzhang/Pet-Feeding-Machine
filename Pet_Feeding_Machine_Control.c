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

unsigned char water_pump_state	= 0x00;	//	0 - NULL	|	1 - Activate
unsigned char set_drawer_status	= 0x00;	//	0 - NULL	|	1 - Activate

uint16_t water_sensor_adc;
uint16_t force_sensing_resistor_adc;
uint16_t photoresistor_adc;

uint16_t in_weight;
uint16_t eat_value;
//----------Bluetooth----------
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
			in_weight = 0;
			eat_value = 35;	// 50g
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
			if (USART_HasReceived(0))
			{
				command = USART_Receive(0);
				BUR_state = PROCESSING;
			}
			break;

		case PROCESSING:
			// Activate Drawer
			if (command & 0x02)
			{
				set_drawer_status = 0x01;
				in_weight = force_sensing_resistor_adc;
			}
			
			// Set Eat Value
			if (command & 0x10)
			{
				eat_value = 35;		// 50g
			}
			else if (command & 0x10)
			{
				eat_value = 75;		//100g
			}
			else if (command & 0x20)
			{
				eat_value = 105;	//150g
			}
			else if (command & 0x40)
			{
				eat_value = 140;	//200g
			}
			else if (command & 0x80)
			{
				eat_value = 175;	//250g
			}
			command = 0x00;
			BUR_state = BUR_RECIVER;
			break;
		default:

		break;
	}
}

//----------Water Sensor & Force Sensing Resistor & Photoreresistor----------
unsigned char current_drawer_state;
unsigned char drawer_in_mid;
int counter;
enum Water_Sensor_and_Force_Sensing_Resistor_State {SENSOR_INIT, SENSOR_GET, SENSOR_WAIT} SENSOR_state;

void SENSOR_Init(){
	SENSOR_state = SENSOR_INIT;
	adc_init();

	DDRC = 0xFF; PORTC = 0x00;				// LCD data lines
	DDRD = 0xFF; PORTD = 0x00; 				// LCD control lines
	// Initializes the LCD display
	LCD_init();
	LCD_ClearScreen();
	LCD_DisplayString(1, "Water ADC:      Load  ADC:      ");
}

void SENSOR_Tick(){
	static unsigned char *display_string;	//	For Display on LCD

	static unsigned char begin_time[3];

	//Actions
	switch(SENSOR_state){
		case SENSOR_INIT:
			display_string = "Water ADC:      Load  ADC:      ";
			force_sensing_resistor_adc = 0;
			water_sensor_adc = 0;
			current_drawer_state = 0x01;
			drawer_in_mid = 0x00;

			begin_time[0] = 0x00;
			begin_time[1] = 0x00;
			begin_time[2] = 0x00;

			counter = 0;
			break;

		case SENSOR_GET:
			// Get Average ADCs
			force_sensing_resistor_adc = 0;
			water_sensor_adc = 0;
			for (int i = 0; i < 32; i++)
			{
				// Get Water Level Sensor ADC
				water_sensor_adc += ReadADC3(0);
				// Get Force Sensing Resistor ADC
				force_sensing_resistor_adc += ReadADC3(1);
				// Get Photoresistor ADC
				photoresistor_adc += ReadADC3(2);
			}
			force_sensing_resistor_adc = (uint16_t) (force_sensing_resistor_adc / 32);
			water_sensor_adc = (uint16_t) (water_sensor_adc / 32);
			photoresistor_adc = (uint16_t) (photoresistor_adc / 32);


			//	Check Water Sensor ADC
			if ((int)(water_sensor_adc) < 750)
			{
				water_pump_state = 0x01;
			}
			else if ((int)(water_sensor_adc) > 780)
			{
				water_pump_state = 0x00;
			}
			// Check Photoresistor ADC
			if ((int)(photoresistor_adc) >= 1025)
			{
				current_drawer_state = 0x00;
				drawer_in_mid = 0x00;
			}
			else if ((int)(photoresistor_adc) <= 800)
			{
				current_drawer_state = 0x01;
				drawer_in_mid = 0x00;
			}
			else
			{
				drawer_in_mid = 0x01;
			}
			// Check Force Sensing Resistor ADC
			if (current_drawer_state == 0x00)
			{
				if (set_drawer_status && force_sensing_resistor_adc > eat_value + 150)
				{
					set_drawer_status = 0x01;
				}
				else
				{
					set_drawer_status = 0x00;
				}
			}
			else
			{
				if ((force_sensing_resistor_adc - in_weight) > eat_value)
				{
					set_drawer_status = 0x01;
				}
				else
				{
					set_drawer_status = 0x00;
				}
			}
			
			// Display to LCD
			unsigned char str_one[6] = { 0 };
			sprintf(str_one, "%d", (int) water_sensor_adc);

			unsigned char str_two[6] = { 0 };
			sprintf(str_two, "%d", (int) force_sensing_resistor_adc);
			//sprintf(str_two, "%d", (int) photoresistor_adc);

			for (int i = 0; i < 6; ++i)
			{
				// Display Water Sensor ADC
				LCD_Cursor(11+i);
				if (str_one[i])
				{
					LCD_WriteData(str_one[i]);
				}
				else
				{
					LCD_WriteData(' ');
				}
				// Display Force Sensing Resistor ADC
				LCD_Cursor(27+i);
				if (str_two[i])
				{
					LCD_WriteData(str_two[i]);
				}
				else
				{
					LCD_WriteData(' ');
				}
			}
			LCD_Cursor(32);

			LCD_WriteData('0' + counter);

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

//----------USART SENDER----------
enum USART_SENDER_State {US_INIT, US_SENDER, US_ISSEND} US_state;

void US_Init()
{
	// Initializes the desired USART.
	initUSART(1);
	US_state = US_INIT;
}

void US_Tick()
{
	static unsigned char us_command;
	// Action
	switch(US_state)
	{
		case US_INIT:
			us_command = 0x00;
			break;

		case US_SENDER:
			us_command = 0x00;
			
			if (water_pump_state)
			{
				us_command = 0x01;
			}
			else
			{
				us_command = 0x00;
			}
			
			if (drawer_in_mid)
			{
				us_command |= 0x02;
			}
			else if (set_drawer_status)
			{
				us_command |= 0x02;
				if (current_drawer_state)
				{
					us_command |= ~0x04;
				}
				else
				{
					us_command &= ~0x04;
				}
			}
			else
			{
				us_command &= ~0x02;
			}
			
			counter = us_command;
			break;

		case US_ISSEND:

			break;

		default:

			break;
	}
	// Transitions
	switch(US_state)
	{
		case US_INIT:
			US_state = US_SENDER;
			break;

		case US_SENDER:
			if (USART_IsSendReady(1))
			{
				USART_Send(us_command, 1);
				US_state = US_ISSEND;
			}
			break;

		case US_ISSEND:
			if (USART_HasTransmitted(1))
			{
				US_state = US_SENDER;
			}
			break;
		default:

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
		vTaskDelay(1000);
	}
}

void Machine_Control_Task()
{
	BUR_Init();
	SENSOR_Init();
	US_Init();
	for(;;)
	{
		BUR_Tick();
		SENSOR_Tick();
		US_Tick();
		vTaskDelay(100);
	}
}

//----------System Time Pulses----------
void System_Time_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(System_Time_Task, (signed portCHAR *)"System_Time_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Machine Control Pulses----------
void Machine_Control_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Machine_Control_Task, (signed portCHAR *)"Machine_Control_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Main----------
int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;

	DDRB = 0xFF; PORTB = 0x00;

	//Start Tasks
	System_Time_Pulse(2);
	Machine_Control_Pulse(1);
	vTaskStartScheduler();
	
	return 0;
}