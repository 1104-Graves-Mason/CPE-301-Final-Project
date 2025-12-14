//CPE 301 Final Project
//Mason Graves & David Recanzone
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <Stepper.h>
#include <dht.h>

//ISR
volatile bool startFlag = false;
volatile unsigned long lastInterruptTime = 0;
volatile unsigned char* pin_d = (unsigned char*) 0x29;
volatile unsigned char* ddr_d = (unsigned char*) 0x2A;
const unsigned long debounceDelay = 50;

//Stop and Reset Buttons
volatile unsigned char* pin_l = (unsigned char*) 0x109;
volatile unsigned char* ddr_l = (unsigned char*) 0x10A;

//dht
#define DHT11_PIN 52
dht DHT;
int t_threshold = 24;
int w_threshold = 30;

//gpio
volatile unsigned char* pin_h = (unsigned char*) 0x100;
volatile unsigned char* ddr_h = (unsigned char*) 0x101;
volatile unsigned char* port_h = (unsigned char*) 0x102;
volatile unsigned char* ddr_b = (unsigned char*) 0x24;
volatile unsigned char* port_b = (unsigned char*) 0x25;

//usart
#define RDA 0x80
#define TBE 0x20 
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//ADC
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//Timers
volatile unsigned char *myTCCR1A  = 0x80;
volatile unsigned char *myTCCR1B  = 0x81;
volatile unsigned char *myTCCR1C  = 0x82;
volatile unsigned char *myTIMSK1  = 0x6F;
volatile unsigned char *myTIFR1   = 0x36;
volatile unsigned int  *myTCNT1   = 0x84;
int currentTicks = 65536;

//rtc, lcd
RTC_DS1307 rtc;
LiquidCrystal lcd(32, 30, 28, 26, 24, 22);
int temperature = 100;
int humidity = 10;

//stepper motor
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 5,3,4,2);

//fan motor
int lastCycleStart = 0;
unsigned long motorPeriod = 30;
double dutyCycle = 0.5;

//millis
unsigned long previousMillis = 0;
unsigned long motorMillis = 0;
const long interval = 60000;
const float stepInterval = 0.00294;

//test
const int waterSensor = A0;
bool water = true;


void setup(){
  //baud rate and ADC initializer
  U0init(9600);
  adc_init();

  //pointer initializer for motors
  *ddr_h &= 0xE7;
  *ddr_h |= 0x60;
  *ddr_b |= 0x70;

  //pointer initializers for interrupt
  *ddr_d &= 0xF7;
  *ddr_l &= 0x3F;
  attachInterrupt(digitalPinToInterrupt(18), startButton, RISING);

  //begin real time clock, stepper motor, LCD
  rtc.begin();
  lcd.begin(16,2);
  myStepper.setSpeed(10);
}

void loop() {
  //input pointer initializers
  bool pin6 = *pin_h & 0x08;
  bool pin7 = *pin_h & 0x10;
  bool pin42 = *pin_l & 0x80;
  bool pin43 = *pin_l & 0x40;

  //start flag ISR
  if(startFlag){
    //start millis counter, read adc
    unsigned long currentMillis = millis();
    int val = adc_read(0);

    //motor controls
    if(pin6 && pin7){
    }else if(pin6){
      if(currentMillis - motorMillis >= stepInterval) {
        motorMillis=currentMillis;
        myStepper.step(1);
      }
    }else if(pin7){
      if(currentMillis - motorMillis >= stepInterval) {
        motorMillis=currentMillis;
        myStepper.step(-1);
      }
    }

    //reset button
    if(pin43){
      water = true;
    }

    //to be updated every minute
    if(currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      //read DHT
      int chk = DHT.read11(DHT11_PIN);
      temperature = DHT.temperature;
      humidity = DHT.humidity;

      //water sensor updater
      if(water == true){
        if(val < w_threshold){
          water = false;
        }else{
          //if water is high enough, check temperature
          if(currentMillis - lastCycleStart >= motorPeriod){
            lastCycleStart = currentMillis;
          }
          water = true;
          lcd.clear();
          if(temperature > t_threshold){
            LED('b');
            *port_h |= 0x20;
          }else{
            LED('g');
            *port_h &= 0xDF;
          }
          lcd.clear();
          lcdUpdate();
        }
      //Red state, water level too low
      }else{
        LED('r');
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Water level");
        lcd.setCursor(0,1);
        lcd.print("is too low.");
      }
    }

  //disabled state
  }else{
    lcd.clear();
    LED('y');
    *port_h &= 0xDF;
  }

  //stop button
  if(pin42){
    startFlag = false;
  }
}

void adc_init() //write your code after each commented line and follow the instruction 
{
  // setup the A register
  // set bit 7 to 1 to enable the ADC 
  *my_ADCSRA |= 0x80;
  // clear bit 5 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0xDF;
  // clear bit 3 to 0 to disable the ADC interrupt 
  *my_ADCSRA &= 0xF7;
  // clear bit 0-2 to 0 to set prescaler selection to slow reading
  *my_ADCSRA &= 0xF8;
  // setup the B register
  // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0xF7;
  // clear bit 2-0 to 0 to set free running mode
  *my_ADCSRB &= 0xF8;
  // setup the MUX Register
  // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX &= 0x7F;
  // set bit 6 to 1 for AVCC analog reference
  *my_ADMUX |= 0x40;
  // clear bit 5 to 0 for right adjust result
  *my_ADMUX &= 0xDF;
  // clear bit 4-0 to 0 to reset the channel and gain bits
  *my_ADMUX &= 0xE0;
}

unsigned int adc_read(unsigned char adc_channel_num) //work with channel 0
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX &= 0xE0;

  // clear the channel selection bits (MUX 5) hint: it's not in the ADMUX register
  *my_ADCSRB &= 0xF7;
 
  // set the channel selection bits for channel 0
  *my_ADMUX |= 0x00;

  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;

  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register and format the data based on right justification (check the lecture slide)
  unsigned char low = *(volatile unsigned char*)0x78;   // ADCL
  unsigned char high = *(volatile unsigned char*)0x79;  // ADCH
  unsigned int val = (high << 8) | low;
  return val;
}

//LCD Functions
void lcdUpdate(){
  //LCD time printer
  lcd.setCursor(0,0);
  DateTime now = rtc.now();
  lcd.print(now.month());
  lcd.print('/');
  lcd.print(now.day());
  lcd.print('/');
  int year = now.year()%100;
  lcd.print(year);
  lcd.print(' ');
  lcd.print(now.hour());
  lcd.print(':');
  lcdPrint2(now.minute());

  lcd.setCursor(0,1);
  lcd.print("T: ");
  lcd.print(temperature);
  lcd.print(" H: ");
  lcd.print(humidity);
}

//prints minutes as two digit number
void lcdPrint2(int value) {
  if(value < 10){
    lcd.print("0");
    lcd.print(value);
  }
  else{
    lcd.print(value);
  }
}

//USART
void U0init(int U0baud){
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit(){
  return *myUCSR0A & RDA;
}
unsigned char U0getchar(){
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata){
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}
void printstring(String word){
  for(int i = 0; i < word.length(); i++){
    U0putchar(word[i]);
  }
}

//ISR
void startButton(){
  startFlag = true;
}

//LED function
void LED(char c){
  if(c == 'r'){
    *port_h &= 0xBF;
    *port_b &= 0xCF;
    *port_b |= 0x40;
  }else if(c == 'y'){
    *port_h &= 0xBF;
    *port_b &= 0xAF;
    *port_b |= 0x20;
  }else if(c == 'g'){
    *port_h &= 0xBF;
    *port_b &= 0x9F;
    *port_b |= 0x10;    
  }else if(c == 'b'){
    *port_h |= 0x40;
    *port_b &= 0x8F;
  }
}