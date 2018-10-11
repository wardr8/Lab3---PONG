#define F_CPU	16000000

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <util/delay.h>
#include "lcd.h"
#include "uart.h"
#include <time.h>

#define FREQ 16000000
#define BAUD 9600
#define HIGH 1
#define LOW 0
#define BUFFER 1024
#define BLACK 0x000001

#define set(reg,bit)		(reg) |= (1<<(bit))
#define clear(reg,bit)		(reg) &= ~(1<<(bit))
#define toggle(reg,bit)		(reg) ^= (1<<(bit))
#define check(reg,bit)		(bool)(reg & (1<<(bit)))

char displayChar = 0;
unsigned int Xcord, Ycord; 
unsigned int player_count = 0;
unsigned int acc_en = 0;
volatile unsigned int x_val, y_val, prev1, prev2=32-4, player1_y_val, player2_y_val;

//-----Ball Physics-----//

#define MAX_X 127		//max x dimension of screen
#define MAX_Y 63		//max y
#define ball_radius 3	//ball radius in pixels
#define RAND_MAX  3

int nX = 64;		//start at middle
int nY = 32;
int nXDir;	//initial direction of travel & speed
int nYDir;

int Acc_cord = 0;
int y_acc_val = 0;

char score[4] = {'0', '1', '2', '3'};
int player1 = 0;
int player2 = 0;

//-----BUZZER-----//
int pcnt = 3;
char HiorLo;
unsigned int Delay;

long map(long x, long in_min, long in_max, long out_min, long out_max)		//mapping function
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

ISR(TIMER1_COMPA_vect){		//interrupt used to control PWM frequency to Buzzer and play sounds
	if (HiorLo){
		OCR1A += Delay;
		HiorLo = 0;
	}
	else {
		OCR1A += Delay;
		HiorLo = 1;
	}
	pcnt--;
	if (pcnt == 0) {
		TIMSK1 = 0;
		TCCR1A = 0;
		pcnt = 3;
	}
}

	
void select_player_count(void)		//determines mode that player wants to play in
{
	char buffer[]="Mode?";
	int i=0;
	clear_buffer(buff);
	player_count = 0;

	while(player_count==0)
	{
		while(buffer[i])
		{
			drawchar(buff, (i*6)+12, 2, buffer[i]);
			i++;
		}
		
		drawchar(buff, 20, 4, '1');
		drawchar(buff, 60, 4, '2');
		drawchar(buff, 100, 4, 'A');
		
		write_buffer(buff);
		
		set(PINC, 1);
		get_cord();
		
		if((x_val < 40)&&(x_val > 0)){
			acc_en = 0;
			player_count = 1;
		}

		else if((x_val < 80)&&(x_val > 40)){
			acc_en = 0;
			player_count = 2;
		}

		else if ((x_val >= 80)&&(x_val < 120)){
			acc_en = 1;
			player_count =1;
		}
		clear(PINC, 1);
	}
}

//-----MAIN FUNCTION-----//

int main(void)
{
	srand(1);
	nXDir = rand()%6-3;	//initial direction of travel & speed generated randomly
	nYDir = rand()%6-3;
	if (nYDir == 0){
		nYDir = 1;
	}
	if (nXDir == 0){
		nXDir = 1;
	}
	
	uart_init();
	//printf("RandY = %d  RandX = %d \n", nYDir, nXDir);	
	
	//setting up the gpio for backlight
	DDRD |= 0x80;
	clear(PORTD, 7);	//RED LED
	
	DDRB |= 0x03;
	clear(PORTB, 1);		//BLUE LED
	clear(PORTB, 0);		//GREEN LED
	
	//-----BUZZER-----//
	
	set(DDRB, 2);		//set BUZZER as output
		
	TCCR1A |= 0x10;		// toggle on OCR1A
	TCCR1B |= 0x0B;		// /64 prescaler, CTC mode
	OCR1A = 0;
	sei();
	
	//lcd initialization
	lcd_init();
	lcd_command(CMD_DISPLAY_ON);
	lcd_set_brightness(0x18);
	write_buffer(buff);
	_delay_ms(1000);
	clear_buffer(buff);
	clear_screen();
	
	//Determine mode
	select_player_count();


	while (1)
	{
		clear_buffer(buff);		//clear screen at beginning of loop
		
		
		//digital input setup for touch
		clear(DDRC, 0);		//set X- (A0) as input
		set(DDRC, 2);		//set X+ (A2) as output
		clear(DDRC, 1);		//set Y- as input
		clear(DDRC, 3);		//set Y+ as input
		
		clear(PORTC, 2);	//set X+ low
		set(PORTC, 1);		//enable internal pull-up on Y-
		
		drawGRID();	

		move_Player1paddle(prev1);		//maintain paddle previous state		
		move_Player2paddle(prev2);		//maintain paddle previous state
		write_buffer(buff);

		if(player_count==1)		// Mode 1, player vs. CPU
		{
			if(nX > 63)
			{
				if((prev2+4)>nY)		//CPU control 
				prev2--;
				else if((prev2+4)<nY)
				prev2++;
				player2_y_val = prev2;
			}
			move_Player2paddle(player2_y_val);
		}

		if(acc_en == 1){		//Accelerometer mode
			get_Acc_cord();
			move_Player1paddle(y_acc_val);
			prev1 = y_acc_val;
			player1_y_val = y_acc_val;
		}

		if(acc_en == 0){	//two player mode
			if (check(PINC, 1)==0){
				get_cord();
			
				if(x_val < 63){
					move_Player1paddle(y_val);
					prev1 = y_val;
					player1_y_val = y_val;
				}
				else if (x_val >= 63){
					if(player_count==2)
					{
						move_Player2paddle(y_val);
						prev2 = y_val;
						player2_y_val = y_val;
					}
				}	
			}
			else if (check(PINC, 1)==1){
				//do nothing
			}
		}

		move_ball();	//ball physics function

		
		write_buffer(buff);		//WRITE buffer once at ened of each loop
		_delay_ms(10);		//effects speed of ball
	}
}

void get_cord(){	//Get X & Y ADC values for each touch
	//Step A - set X- X+ to digital mode, Y- Y+ to ADC
	set(DDRC, 0);		//set X- ADC0 (A0) as output
	set(DDRC, 2);		//set X+ (A2) as output
	clear(DDRC, 1);		//set Y- as input
	clear(DDRC, 3);		//set Y+ as input
	
	set(PORTC, 0);		//set X- high
	clear(PORTC, 2);	//set X+ low
	
	clear(ADMUX, MUX3);	//ADC1 - pin C1 = Y-
	clear(ADMUX, MUX2);
	clear(ADMUX, MUX1);
	set(ADMUX, MUX0);
	
	ADC_init();
	//set(DIDR0, ADC1D);	//disable digital input for ADC1
	while(check(ADCSRA, ADIF) == 0);
	Xcord = ADC;
	set(ADCSRA, ADIF);		//reset flag
	
	//Step B - set Y- Y+ to digital mode, X- X+ to ADC
	clear(DDRC, 0);		//set X- as input
	clear(DDRC, 2);		//set X+ as input
	set(DDRC, 1);		//set Y- as output
	set(DDRC, 3);		//set Y+ as output
	
	set(PORTC, 1);		//set Y- high
	clear(PORTC, 3);	//set Y+ low
	
	clear(ADMUX, MUX3);	//ADC0 - pin C0 = X-
	clear(ADMUX, MUX2);
	clear(ADMUX, MUX1);
	clear(ADMUX, MUX0);
	
	ADC_init();
	//set(DIDR0, ADC0D);	//disable digital input for ADC0
	while(check(ADCSRA, ADIF) == 0);
	Ycord = ADC;
	set(ADCSRA, ADIF);		//reset flag
	
	//printf("Xcord = %u Ycord = %u \n", Xcord, Ycord);
	if (Xcord <= 150) Xcord = 150;
	if (Xcord >= 890) Xcord = 890;
	if (Ycord <= 250) Ycord = 250;
	if (Ycord >= 820) Ycord = 820;
	x_val = map(Xcord, 150, 890, 127, 0);
	y_val = map(Ycord, 250, 820, 8, 55);
	//printf("X_val = %u Y_val = %u \n", x_val, y_val);
}

void get_Acc_cord(void)		//acceleromter ADC data
{
	clear(DDRC, 4);		//set X as input

	clear(ADMUX, MUX3); //ADC4 
	set(ADMUX, MUX2);
	clear(ADMUX, MUX1);
	clear(ADMUX, MUX0);
	
	ADC_init();
	while(check(ADCSRA, ADIF) == 0);
	Acc_cord = ADC;
	set(ADCSRA, ADIF);		//reset flag
	//printf("Acc_cord = %u\n", Acc_cord);
	if (Acc_cord <= 270) Ycord = 270;
	if (Acc_cord >= 400) Ycord = 400;
	y_acc_val = map(Acc_cord, 270, 400, 55, 8);
	//printf("Acc_cord_map = %u\n", x_acc_val);
}


void ADC_init(){	//initialize ADC
	set(ADMUX, REFS0);	//setting reference voltage for ADC to Vcc
	clear(ADMUX, REFS1);
	
	set(ADCSRA, ADPS2);	//set ADC prescaler to 125 kHz based on system clock
	set(ADCSRA, ADPS1);
	set(ADCSRA, ADPS0);
	
	clear(ADCSRA, ADATE);	//no auto trigger
	
	set(ADCSRA, ADEN);	//enable ADC subsystem
	set(ADCSRA, ADSC);	//begin conversion
	
}

void move_Player1paddle(uint8_t y){
	drawline(buff, 2, y-8, 2, y+8, 1);
	drawline(buff, 3, y-8, 3, y+8, 1);
}

void move_Player2paddle(uint8_t y){
	drawline(buff, 124, y-8, 124, y+8, 1);
	drawline(buff, 125, y-8, 125, y+8, 1);
}

void move_ball(void){
	
	nX += nXDir;	//move ball function
	nY += nYDir;
					    
	if(nX <= ball_radius)
	{
		if (player1_y_val >= (nY-10) && player1_y_val <= (nY+10)){
		nX = ball_radius;
		nXDir = abs(nXDir);
		TIMSK1 |= 0x02;
		TCCR1A |= 0x10;
		Delay = 5;
		}
		else{
			toggle(PORTB, 0);
			toggle(PORTB, 1);
			
			TIMSK1 |= 0x02;
			TCCR1A |= 0x10;
			Delay = 1;
			
			_delay_ms(1000);
			PLAYER1_LOSE();
			_delay_ms(1000);
			toggle(PORTB, 0);
			toggle(PORTB, 1);
			
		}
	}
	if(nY <= ball_radius)
	{
		nY = ball_radius;
		nYDir = abs(nYDir);
		TIMSK1 |= 0x02;
		TCCR1A |= 0x10;
		Delay = 5;
	}		    
	if(nX >= (MAX_X - ball_radius))
	{
		if (player2_y_val >= (nY-10) && player2_y_val <= (nY+10)){
			nX = MAX_X - ball_radius;
			nXDir = -nXDir;
			TIMSK1 |= 0x02;
			TCCR1A |= 0x10;
			Delay = 5;
		}
		else{	
			toggle(PORTB, 0);
			toggle(PORTB, 1);
			
			TIMSK1 |= 0x02;
			TCCR1A |= 0x10;
			Delay = 1;
			
			_delay_ms(1000);
			PLAYER2_LOSE();
			_delay_ms(1000);
			toggle(PORTB, 0);
			toggle(PORTB, 1);
		}
		
	}
	if(nY >= (MAX_Y - ball_radius))
	{
		nY = MAX_Y - ball_radius;
		nYDir = -nYDir;
		TIMSK1 |= 0x02;
		TCCR1A |= 0x10;
		Delay = 5;
	}
	
	fillcircle(buff, nX, nY, ball_radius, 1);
					    
}

void drawGRID(void)
{
	for (int i=0; i < 64; i+=8){
		drawline(buff, 64, i, 64, (i+4), 1);	//dashed middle line
	}
	drawline(buff, 0, 0, 127, 0, 1);		//border
	drawline(buff, 127, 0, 127, 63, 1);
	drawline(buff, 0, 0, 0, 63, 1);
	drawline(buff, 0, 63, 127, 63, 1);
	
	drawrect(buff, 32, 6, 11, 11, 1);		//setup score boxes
	drawrect(buff, 86, 6, 11, 11, 1);
	
	drawchar(buff, 35, 1, score[player1]);	//score player 1
	drawchar(buff, 89, 1, score[player2]);
	
}

void PLAYER2_LOSE(void)
{
	//give player 1, one point
	player1 += 1;
	if (player1 > 3){
		player1 = 0;
		player2 = 0;
		drawchar(buff, 27, 4, 'W');
		drawchar(buff, 33, 4, 'I');
		drawchar(buff, 39, 4, 'N');
		write_buffer(buff);
	}
		
	//reset ball to middle
	nX = 64;
	nY = 32;
	nXDir = -abs(rand()%6-3);	//initial direction of travel & speed generated randomly
	nYDir = rand()%6-3;
	if (nYDir == 0){
		nYDir = 1;
	}
	if (nXDir == 0){
		nXDir = 1;
	}

	//begin again

}

void PLAYER1_LOSE(void)
{
	player2 += 1;
	if (player2 > 3){
		player2 = 0;
		player1 = 0;
		drawchar(buff, 81, 4, 'W');
		drawchar(buff, 87, 4, 'I');
		drawchar(buff, 93, 4, 'N');
		write_buffer(buff);
	}

	
	fillcircle(buff, 64, 32, ball_radius, 1);
	nX = 64;
	nY = 32;
	nXDir = abs(rand()%6-3);	//initial direction of travel & speed generated randomly
	nYDir = rand()%6-3;
	if (nYDir == 0){
		nYDir = 1;
	}
	if (nXDir == 0){
		nXDir = 1;
	}
	//begin again
}

