
#include "RTCm41t00slib.h"
#include <U8g2lib.h>
#include <stdint.h>
#include <EEPROM.h>
#include <IRremote.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define UI_BUFFER_SIZE 64
#define SERIAL_TERMINATOR '\n'
#define TMP112_ADDR  0x49
#define IRPIN 21
#define SYSTMMAX 30000

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

RTC_M41T00S rtc;
IRrecv irrecv(IRPIN);
decode_results results;
DateTime nowtime;

volatile bool pressedButton, n, ledstatus, batSmp, tempSmp, buzzSmp, timeSmp;
volatile uint8_t key;
char daysOfTheWeek[7][10] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
char dateString[35];
char ui_buffer[UI_BUFFER_SIZE];
uint16_t alarmtime1,alarmtime2;
const uint8_t kBuzzerPin = 15;
const uint8_t kPowerSwitch = 10;
const int kLightPin = 3;
const uint8_t kButtonPin = 7;
const uint8_t kExtPowerPin = 22;
const uint8_t kLedPin = 13;
const int kBatteryPin = A0;
volatile uint16_t nSysT = 0;
uint16_t nProtT = 0;
uint16_t st = 0;
uint16_t nShutT = 0;

static uint8_t cBatSmp = 0;
static uint8_t nhours = 0;
static int nBatsum = 0;
static int batVal = 0;
static float tempVal = 0;
//static uint16_t  nTimeTmp = 0;


void WakeUp() {
  pressedButton = true;
  digitalWrite(kPowerSwitch, LOW);
  while (!digitalRead(kButtonPin)) {};
  detachInterrupt(digitalPinToInterrupt(kButtonPin));
}

void setup() {
  pinMode(kButtonPin, INPUT_PULLUP);
  pinMode(kExtPowerPin, INPUT);
  pinMode(kBuzzerPin, OUTPUT);
  pinMode(kPowerSwitch, OUTPUT);
  pinMode(kLightPin, OUTPUT);
  pinMode(kLedPin, OUTPUT);
  digitalWrite(kPowerSwitch, HIGH);
  digitalWrite(kBuzzerPin, LOW);
  digitalWrite(kPowerSwitch, LOW);
  digitalWrite(kLightPin, LOW);
  analogReference(INTERNAL2V56);
  Serial.begin(38400);
  rtc.begin();
  rtc.setCalibration(0xA6);
  u8g2.begin();
  u8g2.enableUTF8Print(); 
  //u8g2.setContrast(0);
  T2_init();
  Tmp112_init();
//  attachInterrupt(EXTERNAL_INT_7, WakeUp, LOW);
//  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 
  adjTimeAlarm();
  EEPROM.get(0x00, alarmtime1);
  key = 0;
  ledstatus = true;
  batSmp = true;
  tempSmp = false;
  buzzSmp = true;
  timeSmp = true;
  irrecv.enableIRIn();
  rtc.printAllBits();
  setBrightness(); 
  delay(50); 
 // analogWrite(kLightPin, 35);
}

void loop() {  
  uint16_t  nTimeTmp = 0;
  unsigned int uctmp = 0;
  nowtime = rtc.now();
  
  if (pressedButton) 
  {
   // while (!digitalRead(kButtonPin)) {};
   // detachInterrupt(digitalPinToInterrupt(kButtonPin));
   // digitalWrite(kPowerSwitch, LOW);
    pressedButton = false;
    delay(10);
    key = 0;
  }
   
  if(batSmp)
    {
      uctmp = analogRead(kBatteryPin);
      delay(20);
      nBatsum += uctmp;
      cBatSmp++;
      if (cBatSmp >= 4)
          {
            batVal = nBatsum >> 2;
            batVal = map(batVal, 430, 565, 0, 100);
            nBatsum = 0;
            cBatSmp = 0;
          }
      batSmp = 0;
      tempSmp = 1;   
    }
    
  else if(tempSmp)
     {
       tempVal = gettemp();
       tempSmp = 0;
       batSmp = 1;
     }
     
   switch (key){
    case 0:
      lcdDisplayAll();
    break;
    case 1:
      lcdDisplayA();
    break;
    case 2:
      key = 0;
    break;
    case 9:
      setLcdOff();
      digitalWrite(kLightPin, LOW);
      digitalWrite(kPowerSwitch, HIGH);
      while (!digitalRead(kButtonPin)) {};
      key = 0;
      st = 0;
      Serial.println(F("Sleep --"));
      delay(200);
      attachInterrupt(digitalPinToInterrupt(kButtonPin), WakeUp, LOW);
      powerdown();
      T2_init();
      sound();
      Serial.println(F("WakeUp--"));
      setLcdOn();
    break;
    
    default:
      key = 0;
   }
   
    if(nowtime.minute() == 30 && nowtime.hour() > 6 && nowtime.hour() < 23 && buzzSmp )
        {
          halfsound();
          buzzSmp = false;
        }
    
    if(nowtime.minute() == 0 && nowtime.hour() > 6 && nowtime.hour() < 23 && buzzSmp ) 
      {
        if (timeSmp)
          {
            nhours = nowtime.twelveHour();
            timeSmp = false;
          }
          cli();
					nTimeTmp = SYSTMMAX + nSysT - nProtT;
					sei();
					if(nTimeTmp >= SYSTMMAX)
					{
						nTimeTmp = nTimeTmp - SYSTMMAX;
					}
          if (nTimeTmp >= 650)
          {
            sound();
            nhours--;
            if(nhours == 0)
              {
                  buzzSmp = false;
                  timeSmp = true;
              }
             cli();
             nProtT = nSysT;
             sei(); 
          } 
      }
      
      checkButton();

      detectIR();

    if (st >= 300 && nowtime.hour() >= 0 && nowtime.hour() < 8 )
   // if (st >= 15)
    {
      key = 9;
    }
   // DateTime future (now + TimeSpan(7,12,30,6));     
}

void checkButton(){
  if (nowtime.minute() != 0 && nowtime.minute() != 30 && buzzSmp == 0) buzzSmp = true;
  if (digitalRead(kButtonPin) == 0) 
    {
      delay(10);
      if (digitalRead(kButtonPin) == 0);
        {           
          key++;
          sound();
        }
     }
         cli();
         nShutT = nSysT;
         sei(); 
         while (!digitalRead(kButtonPin)) {
              uint16_t  nTmp = 0;
              cli();
              nTmp = SYSTMMAX + nSysT - nShutT;
              sei();
                if(nTmp >= SYSTMMAX)
                {
                  nTmp = nTmp - SYSTMMAX;
                }
                 if (nTmp >= 1600) {
                  key = 9;
                  break;
                 }
                };
          if (key > 9) key = 0;
          delay(10);
 }

 void T2_init(){
   cli();
  OCR2 = 0x00;
  TCNT2 =0x00;
  TCCR2 = 0x03;     //0x03 clkI/O/64 per 2ms! 0x04 clkI/O/256  per 8ms!
  TIMSK |= (1 << TOIE2);
  sei(); 
}


ISR(TIMER2_OVF_vect)
{
      nSysT++;
      if(nSysT >= SYSTMMAX)  // after 1min reset 
    {
      nSysT = 0;
    }
    if(!(nSysT % 500))
    {
      st++;
      if (st >= SYSTMMAX) st = 0;
    }
}

void detectIR(){
  if (irrecv.decode(&results)) {
    Serial.println(results.value, HEX);

       switch (results.value) {
         case 0xA32AB931:
         case 0x2F502FD:
              sound();
//              if (lightstatus)
//                  {
//                   digitalWrite(kLightPin, LOW);
//                   lightstatus = false;
//                  } else            
//                        {
//                         analogWrite(kLightPin, lightvalue);
//                         lightstatus = true;
//                        }                       
            break;

         case 0x39D41DC6:
         case 0x2F522DD:
 //     if(lightstatus){
          sound();
//          for (int i = 0; i < 16 ; i++){
//          if(lightvalue == 255)
//          lightvalue = 255;
//          else 
//          lightvalue++;
//          delay(10);
//          analogWrite(kLightPin, lightvalue);
//             }
//         }           
           break;
                     
         case 0xE0984BB6:
         case 0x2F518E7:
          sound();
//      if(lightstatus){
//          for (int i = 0; i < 16 ; i++){
//            if (lightvalue == 0)
//            lightvalue = 0;
//            else
//            lightvalue--;
//            delay(10);
//          analogWrite(kLightPin, lightvalue);
//             }
//         }                  
            break;
            
         case 0x4EA240AE:
            delay(50);
            sound();
//            key++;
//           if(key > 3)
//            key = 0;
//           n = 0;           
            break;

         case 0x4E87E0AB:
              delay(50);
              sound();
            break;

         case 0x371A3C86:
            delay(50);
            sound();
            //DisplayTempHum();
            break;
             
         case 0x143226DB:
            delay(50);
            sound();
           // DisplayAll();
            break;
         }
   irrecv.resume();
   }
  
}
