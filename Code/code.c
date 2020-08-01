#include "lpc214x.h"
#include <stdio.h>
#include "LCD.h"
#include <string.h>
#define uint16_t unsigned int
#define LED_OFF (IO0SET = 1U << 31)
#define LED_ON  (IO0CLR = 1U << 31)
#define COL0 (IO1PIN & 1 <<19)
#define COL1 (IO1PIN & 1 <<18)
#define COL2 (IO1PIN & 1 <<17)
#define COL3 (IO1PIN & 1 <<16)
unsigned char lookup_table[4][4]={{1,2,3,4},
																	{1,2,3,4},
																	{1,2,3,4},
																	{1,2,3,5}
																	};

unsigned char rowsel=0,colsel=0;
int flag=0;
int bill_total = 0;
float grain_calibration = 1.5;//1.5 kg for 1 full revolution of stepper motor
float weight_calibration = 100;
																
typedef struct
{
	unsigned char sec;
  unsigned char min;
  unsigned char hour;
  unsigned char weekDay;
  unsigned char date;
  unsigned char month;
  unsigned int year;  
}rtc_t;

rtc_t rtc; // declare a variable to store date,time


// ISR Routine to blink LED D7 to indicate project working
unsigned  int x=0;
__irq   void  Timer1_ISR(void)
 {
	x = ~x;//x ^ 1;
  if (x)   
    IO0SET  =  1u << 31;   //P0.31  =  1
  else   
    IO0CLR =   1u <<31;   // P0.31  = 0	 
	T1IR  =  0x01; // clear match0 interrupt, and get ready for the next interrupt
  VICVectAddr = 0x00000000 ; //End of interrupt 
 }
 
 void timer1_Init()
{
	T1TCR = 0X00;
	T1MCR = 0X03;  //011 
	T1MR0 = 150000;
	T1TC  = 0X00;
	VICIntEnable = 0x0000020;  //00100000 Interrupt Souce No:5, D5=1
	VICVectAddr5 = (unsigned long)Timer1_ISR;  // set the timer ISR vector address
	VICVectCntl5 = 0x0000025;  // set the channel,D5=1,D4-D0->channelNo-5
	T1TCR = 0X01;
}

void Board_init(){
	IODIR0 = 0x0000F000|1U<<20|1U<<31;  //set pins PO.12 to PO.15 as outputs and PO.20 as RS- used in LCD
	IODIR1 |= 1U<<25;  //set pin PO.25 as EN - used in LCD
	//IODIR0 = 0x000F0000; // rows of keyboard
	//IODIR1 = 0x000F0000; //columns of keyboard
	IODIR0 |= 0x3B; //set P0.2-P0.5 as outputs for stepper motor
	
	PINSEL1 = 1U<<18;  //set P0.25 as ADC
	PINSEL0 |= 0x00000005;  // P0.0 & P0.1 ARE CONFIGURED AS TXD0 & RXD0  to connect with rpie and for demonstration
	PINSEL0 |= 0x00050000;  //P0.8 & P0.9 are configured as TXD1 & RXD1 to connect with bluetooth module for bill printer
}	

void uart_init(void)
{
 //configurations to use serial port
 
 U0LCR = 0x83;   /* 8 bits, no Parity, 1 Stop bit    */
 U0DLM = 0; U0DLL = 8; // 115200 baud rate,PCLK = 15MHz
 U0LCR = 0x03;  /* DLAB = 0                         */
 U1LCR = 0x83;   /* 8 bits, no Parity, 1 Stop bit    */
 U1DLM = 0; U1DLL = 8; // 115200 baud rate,PCLK = 15MHz
 U1LCR = 0x03;  /* DLAB = 0                         */
}

void serialPrint(unsigned val)
{
	int i=0;
	unsigned char buf[50],ch;
	sprintf((char *)buf,"%d\x0d\xa",val);
	while((ch = buf[i++])!= '\0')
	  {
		while((U0LSR & (0x01<<5))== 0x00){}; 
    U0THR= ch;                         
	  }
}

void serialPrintStr(char * buf)
{
	int i=0;
	char ch;
	while((ch = buf[i++])!= '\0')
	  {
		  while((U0LSR & (1u<<5))== 0x00){}; 
      U0THR= ch;   
	  }
	//send new line
	//while(U0LSR & (0x01<<5)){};U0THR = 13;
	//while(U0LSR & (0x01<<5)){};U0THR = 10;	
	
}


void billprint(char * buf)
{
	int i=0;
	char ch;
	while((ch = buf[i++])!= '\0')
	  {
		  while((U1LSR & (1u<<5))== 0x00){}; 
      U1THR= ch;   
	  }
	//send new line
	//while(U1LSR & (0x01<<5)){};U1THR = 13;
	//while(U1LSR & (0x01<<5)){};U1THR = 10;	
	
}

int keyboard(){
 IO0DIR |=  0x00FF0000; // to set P0.16 to P0.23 as o/ps

while(1)
 {
//check for keypress in row0,make row0 '0',row1=row2=row3='1'
rowsel=0;IO0SET = 0X000F0000;IO0CLR = 1 << 16;
if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;};
if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;};

//check for keypress in row1,make row1 '0'
rowsel=1;IO0SET = 0X000F0000;IO0CLR = 1 << 17;
if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;};
if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;};

//check for keypress in row2,make row2 '0'
rowsel=2;IO0SET = 0X000F0000;IO0CLR = 1 << 18;//make row2 '0'
if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;};
if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;};

//check for keypress in row3,make row3 '0'
rowsel=3;IO0SET = 0X000F0000;IO0CLR = 1 << 19;//make row3 '0'
if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;};
if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;};
 };

delay_ms(50); //allow for key debouncing
while(COL0==0 || COL1==0 || COL2==0 || COL3==0);//wait for key release
delay_ms(50); //allow for key debouncing
IO0SET = 0X000F0000; //disable all the rows
//U0THR = lookup_table[rowsel][colsel]; //send to serial port(check on the terminal)
serialPrint(lookup_table[rowsel][colsel]);
return lookup_table[rowsel][colsel];
}

void Stepper_motor(unsigned int direction,unsigned int steps)
{
	
	if(direction==1)   //clockwise
	{
					serialPrintStr("\nStepper Motor is running in clockwise direction!\n");
					do{
							IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<2;delay_ms(10);if(--steps == 0) break;
							IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<3;delay_ms(10);if(--steps == 0) break;
							IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<4;delay_ms(10);if(--steps == 0) break;
							IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<5;delay_ms(10);if(--steps == 0) break;
						}while(1);
	}
	
		
		
	else if(direction==0)
	{
		serialPrintStr("\nStepper Motor is running in anticlockwise direction!\n");
		do{
			IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<5;delay_ms(10);if(--steps == 0) break;
			IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<4;delay_ms(10);if(--steps == 0) break;
			IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<3;delay_ms(10);if(--steps == 0) break;
			IO0CLR = 1<<2|1<<3|1<<4|1<<5;IO0SET = 1<<2;delay_ms(10);if(--steps == 0) break;
		}while(1);
		
	}

		IO0CLR = 1<<2|1<<3|1<<4|1<<5;
		serialPrintStr("\nStepper Motor has stopped running!\n");
	
}

unsigned int adc(int no,int ch)
{
	unsigned int val;
	PINSEL0 |=  0x0F300000;   

  switch (no)        
    {
        case 0: AD0CR   = 0x00200600 | (1<<ch); 
                AD0CR  |= (1<<24) ;            
                while ( ( AD0GDR &  ( 1U << 31 ) ) == 0);
                val = AD0GDR;
                break;
 
        case 1: AD1CR = 0x00200600  | ( 1 << ch );       
                AD1CR |=  ( 1 << 24 ) ;                             
                while ( ( AD1GDR & (1U << 31) ) == 0);
                val = AD1GDR;
                break;
    }
    val = (val  >>  6) & 0x03FF;         // bit 6:15 is 10 bit AD value
    return  val;
}
					

int weight()
{
	//all weights are in integers
	int i, steps;
	unsigned char msg[100];
	LCD_Init();
	LCD_CmdWrite(0x80); LCD_DisplayString("Enter weight");
	sprintf((char *)msg,"\nEnter weight that customer wants: ");
	serialPrintStr((char *)msg);
	int w=keyboard();				//get required weight of the item from keyboard
	if(flag){
	
	steps = (w/grain_calibration)*200;  //default  1.5 kg per revolution
	sprintf((char *)msg,"\n The motor will rotate clockwise by %d degrees\n",(int)(steps*1.8));
	serialPrintStr((char *)msg);
	Stepper_motor(1,steps);
	 
	
	
	}		
	
	else
	{
	do  {  
		
		i = adc(0,4)/weight_calibration; //default 100.0
		LCD_Init();
		LCD_CmdWrite(0x80); LCD_DisplayString((char*)i);
		sprintf((char *)msg,"%d KG\n",i);
	  serialPrintStr((char *)msg);
		

		if(i>w)
		{
			serialPrintStr("\nRemove weight\n");
			delay_ms(1000);
		}
		else if(i<w)
		{
			serialPrintStr("\nAdd weight\n");
			delay_ms(1000);
		}
			
	} while(i!=w);
	
	}
	serialPrintStr("\nWeight Matched!\n");
	flag=0;
	return w;
}



int rate(char name[])
{
	char allnames[20][20] = { "Apple" ,"Mango","Pineapple","Litchi","Banana","Mushroom","Tomatoes","Cabbage","Eggplant","Carrot","Grain"};
	int i=0,allrates[20]={120,70,100,200,30,150,40,30,35,20,25};
	while(i<20){
		if(strcmp(name,allnames[i])==0)
			return allrates[i];
		else
			i++;
	}
	return -1;//error: item not found
}	

void update(char name[],int online)
{
	unsigned char msg[100];
	if(online)
	{
	//Send item name and quantity to rpie to update to cloud
	int a=weight();
	
	sprintf((char *)msg,"Weight:%d KG\n",a);
	serialPrintStr("\n ");//not for rpi
	serialPrintStr((char *)name);
	serialPrintStr("\n ");
	serialPrintStr((char *)msg);//using same UART as the terminal to send to rpi; for all the other serial prints in the program use lcd
	//above 2  serial prints which is shown in uart terminals are  sent to rpi through tx/rx:output shown for convenience
	serialPrintStr("\n ");//not for rpi
	}
	
	else
	{
	int a=weight(),rt,amt;
	billprint("\n ");
	billprint((char *)name);
	billprint("\t ");
	rt = rate(name);
	sprintf((char *)msg,"Rs %d per KG",rt);
	billprint((char *)msg);
	billprint("\t");
	sprintf((char *)msg,"%d KG",a);
	billprint((char *)msg);
	billprint("\t");
	amt = a*rt;
	sprintf((char *)msg,"Rs %d",amt);
	billprint((char *)msg);
	bill_total+=amt;
	billprint("\n");
	serialPrintStr("Enter 1 to print bill, 2 to continue program: ");
	int ch1 = keyboard();
	if(ch1 == 1)
	{
		sprintf((char *)msg,"Total amount due is Rs %d",bill_total);
		billprint((char *)msg);
		
		while(1);//end of program
	}	
	}
}


void calibrate()
{
	int ch,reading,wt;
	float max_wt_readable;
	unsigned char msg[100];
	serialPrintStr("\nEnter 1 for grain stepper motor calibration and 2 for load cell calibration: ");
	ch = keyboard();
	if(ch==2)
	{
		serialPrintStr("\nPlace calibrating weight on weighing machine and press 1: ");
		ch = keyboard();
		if(ch!=1)
			return;
		reading = adc(0,4);
		serialPrintStr("\nEnter actual weight of the item: ");
		wt = keyboard();
		max_wt_readable = (wt/(float)reading) * 1023; //gives max wt in kg
		weight_calibration = 1023/max_wt_readable; //so if max weightt readable is 10.23 KG then weight valibration is its default value 100
		
		sprintf((char *)msg,"\nWeight calibration divisor has been set to %f\n",weight_calibration);
		serialPrintStr((char *)msg);
	}
	else
	{
			serialPrintStr("\nStepper motor will now rotate clockwise by 360 degrees and check the weight\n");
			Stepper_motor(1,200);
			if(adc(0,4)<10)
			{
				serialPrintStr("\nNothing fell on the load sensor. Please check the pipes and try again.\n");
				return;
			}
			grain_calibration = adc(0,4)/weight_calibration;
			sprintf((char *)msg,"\nGrain calibration has been set to %f KG of grain per full rotation.\n",grain_calibration);
			serialPrintStr((char *)msg);
			delay_ms(300);
			serialPrintStr("\nStepper motor will now rotate anti-clockwise by 360 degrees to send the grains back\n");
			Stepper_motor(1,200);
			delay_ms(200);
	}

}



void RTC_Init(void)
{
   //enable clock and select external 32.768KHz
	   CCR = ((1<< 0 ) | (1<<4));//D0 - 1 enable, 0 disable
} 														// D4 - 1 external clock,0 from PCLK

// SEC,MIN,HOUR,DOW,DOM,MONTH,YEAR are defined in LPC214x.h
void RTC_SetDateTime(rtc_t *rtc)//to set date & time
{
     SEC   =  rtc->sec;       // Update sec value
     MIN   =  rtc->min;       // Update min value
     HOUR  =  rtc->hour;      // Update hour value 
     DOW   =  rtc->weekDay;   // Update day value 
     DOM   =  rtc->date;      // Update date value 
     MONTH =  rtc->month;     // Update month value
     YEAR  =  rtc->year;      // Update year value
}

void RTC_GetDateTime(rtc_t *rtc)
{
     rtc->sec     = SEC ;       // Read sec value
     rtc->min     = MIN ;       // Read min value
     rtc->hour    = HOUR;       // Read hour value 
     rtc->weekDay = DOW;      // Read day value 
     rtc->date    = DOM;       // Read date value 
     rtc->month   = MONTH;       // Read month value
     rtc->year    = YEAR;       // Read year value

}


int main()
{
	unsigned char msg[100];
	int k1,k2,k3;
	int online;
	LCD_Reset();
  LCD_Init();
	Board_init();
  uart_init();
	timer1_Init();
	// set date & time to 7thApril 2020,10:00:00am 
	rtc.hour = 17;rtc.min =  00;rtc.sec =  00;//10:00:00am
  rtc.date = 14;rtc.month = 06;rtc.year = 2020;//07th April 2020
  RTC_SetDateTime(&rtc);  // comment this line after first use
	RTC_GetDateTime(&rtc);//get current date & time stamp
	sprintf((char *)msg,"time:%2d:%2d:%2d  Date:%2d/%2d/%2d \x0d\xa",(uint16_t)rtc.hour,(uint16_t)rtc.min,(uint16_t)rtc.sec,(uint16_t)rtc.date,(uint16_t)rtc.month,(uint16_t)rtc.year);
	// use the time stored in the variable rtc for date & time stamping
	serialPrintStr((char*)msg);
 
	LCD_CmdWrite(0x80); LCD_DisplayString("Welcome");
  LCD_CmdWrite(0xc0); LCD_DisplayString("1. Fruits    4.Calibration");
  LCD_CmdWrite(0x94); LCD_DisplayString("2. Vegetables    ");
  LCD_CmdWrite(0xD4); LCD_DisplayString("3. Grains    ");
	
	serialPrintStr("Welcome: Press 1 for online mode and 2 for offline mode: ");
	online = keyboard();
	if(online!=1)
		online = 0;
	
	while(1)
	{
		serialPrintStr("\nChoose:\n 1.Fruits 2.Vegetables 3.Grains 4.Calibration: ");
	k1 = keyboard();
	switch(k1)
	{
		case 1: LCD_Init();
		LCD_CmdWrite(0x80); LCD_DisplayString("Press");
    LCD_CmdWrite(0xc0); LCD_DisplayString("1. Apple    4.Litchi");
    LCD_CmdWrite(0x94); LCD_DisplayString("2. Mango    5.Banana");
    LCD_CmdWrite(0xD4); LCD_DisplayString("3. Pineapple");
		sprintf((char *)msg,"\nPress\n 1.Apple 2.Mango 3.Pineapple 4.Litchi 5.Banana: ");
	  serialPrintStr((char *)msg);
		k2 = keyboard();
			switch(k2)
		{
			case 1: update("Apple",online);
							break;
			case 2: update("Mango",online);
							break;
			case 3: update("Pineapple",online);
							break;
			case 4: update("Litchi",online);
							break;
			case 5: update("Banana",online);
							break;
		}
			break;
		case 2: LCD_Init();
		LCD_CmdWrite(0x80); LCD_DisplayString("Press");
    LCD_CmdWrite(0xc0); LCD_DisplayString("1. Mushroom    4.Eggplant");
    LCD_CmdWrite(0x94); LCD_DisplayString("2. Tomatoes    5.Carrot");
    LCD_CmdWrite(0xD4); LCD_DisplayString("3. Cabbage");
		sprintf((char *)msg,"\nPress\n 1.Mushroom 2.Tomatoes 3.Cabbage 4.Eggplant 5.Carrot: ");
	  serialPrintStr((char *)msg);
		k3 = keyboard();
		switch(k3)
		{
			case 1: update("Mushroom",online);
							break;
			case 2: update("Tomatoes",online);
							break;
			case 3:	update("Cabbage",online);
							break;
			case 4: update("Eggplant",online);
							break;
			case 5: update("Carrot",online);
							break;
		}
		break;
		case 3: LCD_Init();
						flag=1;
						update("Grain",online);
						flag=0;
						break;
		case 4:
		case 5:
						LCD_Init();
						calibrate();
						break;
	}
}
	
}




	
