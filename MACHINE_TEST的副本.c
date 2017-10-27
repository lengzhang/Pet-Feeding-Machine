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
		/*
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
			*/

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

unsigned char drawer_status;	//	0 - in | 1 - out
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
			if (USART_HasReceived(0))
			{
				command = USART_Receive(0);
				BUR_state = PROCESSING;
			}
			break;

		case PROCESSING:
			// Activate Water Pump
			if ((command & 0x01) && activate_water_pump == 0x00)
			{
				activate_water_pump = 0x01;
			}
			// Activate Drawer
			if ((command & 0x02) && activate_drawer == 0x00)
			{
				activate_drawer = 0x01;
				drawer_status = (~drawer_status) & 0x01;
			}
			command = 0x00;
			BUR_state = BUR_RECIVER;
			break;
		default:

		break;
	}
}

//----------Water Sensor & Force Sensing Resistor----------

uint16_t water_sensor_adc;
uint16_t force_sensing_resistor_adc;
uint16_t photoresistor_adc;
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
	//static uint16_t water_sensor_adc;
	//static uint16_t force_sensing_resistor_adc;

	static unsigned char begin_time[3];
	static unsigned char counter;
	//Actions
	switch(SENSOR_state){
		case SENSOR_INIT:
			display_string = "Water ADC:      Load  ADC:      ";
			force_sensing_resistor_adc = 0;
			water_sensor_adc = 0;

			begin_time[0] = 0x00;
			begin_time[1] = 0x00;
			begin_time[2] = 0x00;

			counter = 0x00;
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
				// Get Photoresistor ADC
				photoresistor_adc += ReadADC3(0);
			}
			force_sensing_resistor_adc = (uint16_t) (force_sensing_resistor_adc / 32);
			water_sensor_adc = (uint16_t) (water_sensor_adc / 32);
			photoresistor_adc = (uint16_t) (photoresistor_adc / 32);

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
			unsigned char str_one[6] = { 0 };
			sprintf(str_one, "%d", (int) water_sensor_adc);

			unsigned char str_two[6] = { 0 };
			sprintf(str_two, "%d", (int) force_sensing_resistor_adc);

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
			LCD_Cursor(33);
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

			counter = 0x00;
			SENSOR_state = SENSOR_WAIT;
			break;

		case SENSOR_WAIT:
			/*
			//	Wait for 1 second, so get ADC from sensors every 1 second.
			// 	Since if already waiting 1 second, the system time
			//	must different than the time when start to wait.
			if (begin_time[0] != system_time[0]
				|| begin_time[1] != system_time[1]
				|| begin_time[2] != system_time[2])
			{
				SENSOR_state = SENSOR_GET;
			}
			*/
			counter++;
			if (counter > 0x09)
			{
				SENSOR_state = SENSOR_GET;
			}
			break;

		default:
			SENSOR_state = SENSOR_INIT;
			break;
	}
}

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
					//CLR_BIT(PORTD, 3);
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
unsigned char motor_steps [2][8] =
{
	{0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09},
	{0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01},
};

enum Drawer_State {DRAWER_INIT, DRAWER_CHECK, DRAWER_ONE_PHASE} DRAWER_STATE;

void Drawer_Init()
{
	DRAWER_STATE = DRAWER_INIT;
	activate_drawer = 0x00;
}

void Drawer_Tick()
{
	static unsigned char motor_index;
	static unsigned char counter;
	// Actions
	switch(DRAWER_STATE)
	{
		case DRAWER_INIT:
			drawer_status = 0x00;
			motor_index = 0x00;
			counter = 0x00;
			break;

		case DRAWER_CHECK:
			if (drawer_status & 0x01)
			{
				SET_BIT(PORTD, 4);
			}
			else
			{
				CLR_BIT(PORTD, 4);
			}
			break;
			
		case DRAWER_ONE_PHASE:
			PORTB = motor_steps[drawer_status & 0x01][motor_index];
			break;
		
		default:

			break;
	}
	// Transitions
	switch(DRAWER_STATE)
	{
		case DRAWER_INIT:
			DRAWER_STATE = DRAWER_CHECK;
			break;

		case DRAWER_CHECK:
			/*
			if (activate_drawer == 0x01)
			{
				drawer_status = (~drawer_status) & 0x01;
				activate_drawer = 0x00;
			}
			*/
			if (photoresistor_adc > 500 && drawer_status)
			{
				motor_index = 0x00;
				DRAWER_STATE = DRAWER_ONE_PHASE;
			}
			if (photoresistor_adc < 950 && drawer_status == 0x00)
			{
				motor_index = 0x00;
				DRAWER_STATE = DRAWER_ONE_PHASE;
			}
			break;
			
		case DRAWER_ONE_PHASE:
			motor_index++;
			if (motor_index > 0x07)
			{
				DRAWER_STATE = DRAWER_CHECK;
			}
			break;
		
		default:
			DRAWER_STATE = DRAWER_INIT;
			break;
	}
}

/*
unsigned char temp_B = 0x00;

unsigned char motor_steps [2][8] =
	{
		{0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09},
		{0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01}
	};

//unsigned char motor_steps_A [8] = {0x01,0x03,0x02,0x06,0x04,0x0c,0x08,0x09};
//unsigned char motor_steps_B [8] = {0x09,0x08,0x0c,0x04,0x06,0x02,0x03,0x01};

unsigned char drawer_state;	// 0 - in | 1 - out

enum Drawer_State {D_INIT, D_WAIT, D_MOVE, D_DONE} D_state;

void D_Init(){
	D_state = D_INIT;
	activate_drawer = 0x00;
}

void D_Tick()
{
	static unsigned char motor_index;
	static unsigned char current_motor_phase;
	// Action
	switch(D_state)
	{
		case D_INIT:
			drawer_state = 0x00;
			motor_index = 0x00;
			current_motor_phase = 0;
			break;

		case D_WAIT:
			if (drawer_state)
			{
				SET_BIT(PORTD, 4);
			}
			else
			{
				CLR_BIT(PORTD, 4);
			}
			break;

		case D_MOVE:
			if (motor_index > 0x07)
			{
				motor_index = 0x00;
				current_motor_phase--;
			}
			PORTB = motor_steps[drawer_state][motor_index];
			motor_index++;
			break;

		case D_DONE:
			drawer_state = (~drawer_state) & 0x01;
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
			if (activate_drawer)
			{
				motor_index = 0x00;
				current_motor_phase = 0xFF;
				D_state = D_MOVE;
			}
			break;

		case D_MOVE:
			//	(90 / 5.625) * 40 = 640
			if (current_motor_phase)
			{
				D_state = D_MOVE;
			}
			else
			{
				PORTB = 0x00;
				D_state = D_DONE;
			}
			break;

		case D_DONE:
			activate_drawer = 0x00;
			D_state = D_WAIT;
			break;

		default:
			D_state = D_INIT;
			break;
	}
}
*/

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

void Bluetooth_USART_RECEIVER_Task()
{
	BUR_Init();
	for(;;)
	{
		BUR_Tick();
		vTaskDelay(100);
	}
}

void Sensor_Task()
{
	SENSOR_Init();
	WP_Init();
	for(;;)
	{
		SENSOR_Tick();
		WP_Tick();
		vTaskDelay(100);
	}
}

void Water_Pump_Task()
{
	WP_Init();
	for(;;)
	{
		WP_Tick();
		vTaskDelay(100);
	}
}

void Drawer_Task()
{
	Drawer_Init();
	for (;;)
	{
		Drawer_Tick();
		vTaskDelay(50);
	}
}

//----------System Time Pulses----------
void System_Time_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(System_Time_Task, (signed portCHAR *)"System_Time_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Bluetooth USART RECEIVER Pulses----------
void Bluetooth_USART_RECEIVER_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Bluetooth_USART_RECEIVER_Task, (signed portCHAR *)"Bluetooth_USART_RECEIVER_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Sensor Pulses----------
void Sensor_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Sensor_Task, (signed portCHAR *)"Sensor_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Water Pump Pulses----------
void Water_Pump_Pulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Water_Pump_Task, (signed portCHAR *)"Water_Pump_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Drawer Pulses----------
void DrawerPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(Drawer_Task, (signed portCHAR *)"Drawer_Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
//----------Main----------
int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;

	DDRB = 0xFF; PORTB = 0x00;

	//Start Tasks
	//System_Time_Pulse(5);
	Bluetooth_USART_RECEIVER_Pulse(3);
	Sensor_Pulse(1);
	//Water_Pump_Pulse(2);
	DrawerPulse(4);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}