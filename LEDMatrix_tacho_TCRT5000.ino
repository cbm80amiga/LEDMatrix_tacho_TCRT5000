// Fidget Spinner counter and tachometer
// 08.09.2017 Pawel A. Hernik
// source code for the video:
// https://youtu.be/z6PPWfrt0iY

/*
 Parts:
 - TCRT5000
 - 100ohm and 10kohm resistors
 - MAX7219 LED Matrix
 - Arduino Pro Mini/Nano
 - button
 
 TCRT5000 pinout from the top:
 -------\
 Col  Ano
 Emi  Cat
 -------/
 Ano/Cat - anode/catode of IR LED (blue one)
 Col/Emi - collector/emitter of phototransistor (black one)
 
 TCRT5000 connections:
 Cat and Emi to GND
 Ano through 100 ohm resistor to pin #8
 Col to pin A0, pull up 10k to VCC
*/

#include <Arduino.h>

#define NUM_MAX 4
#define ROTATE  90

#define DIN_PIN 12 
#define CS_PIN  11
#define CLK_PIN 10

#include "max7219.h"
#include "fonts.h"

#define IRLED 8
#define IRREC A0
#define BUTTONPIN 9

// =======================================================================

volatile unsigned long cntTime=0;
volatile unsigned long cnt=0;

// =======================================================================
int oldState = HIGH;
long debounce = 30;
long b1Debounce = 0;
long b1LongPress = 0;

int checkButton()
{
  if(millis() - b1Debounce < debounce)
    return 0;
  int state = digitalRead(BUTTONPIN);
  if(state == oldState) {
    if(state == LOW && (millis() - b1LongPress > 500) ) return -1;
    return 0;
  }
  oldState = state;
  b1Debounce = millis();
  if(state == LOW) b1LongPress = millis();
  return state == LOW ? 1 : 0;
}
// =======================================================================
void setup() 
{
  Serial.begin(9200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN, 1);
  sendCmdAll(CMD_INTENSITY, 0);
  clr();
  refreshAll();
  pinMode(BUTTONPIN,INPUT_PULLUP);
  cntTime = millis();
  pinMode(IRLED,OUTPUT);
  digitalWrite(IRLED,HIGH);
}
// =======================================================================
unsigned long rpm=0,maxrpm=0;
int dispRotTime=0, rotTime=0;
unsigned long measureTime=0,curTime,startTime=0;
int dispCnt=0,measureCnt=0;
char txt[10];
const int resetTime = 2000;
const int minRotNum = 4;  // 1 - calc after every rotation
int mode = 0;
int irState = 0;
int irStateOld = 0;
int irVal;

void loop()
{
  curTime = millis();
  irStateOld = irState;
  irVal = analogRead(IRREC);
  irState = irVal > 200 ? 1 : 0; // 100 good for red
  //Serial.println(irState+"\t"+irVal);
  if(irStateOld!=irState && irState==0) { // falling edge
    cnt++;
    cntTime=curTime; 
  }
  
  if(curTime-cntTime>resetTime) { // reset when less than 30RPM (dt>2s)
    cnt = measureCnt = 0;
    rpm = 0;
  }
  if(cnt==1) startTime = cntTime;
  if(cnt-measureCnt>=minRotNum) {
    rpm = 60000L*(cnt-measureCnt)/(cntTime-measureTime);
//    Serial.println(String(rpm)+"\t"+(cntTime-measureTime)+"\t"+cntTime+"\t"+measureTime);
    measureCnt = cnt;
    measureTime = cntTime;
  }
  rotTime = (cntTime-startTime)/1000; // time in seconds
  if(cnt>1 || !dispRotTime) {  // keep previous time on display until new rotation starts
    dispRotTime = rotTime;
    dispCnt = cnt;
  }
  if(rpm>maxrpm) maxrpm=rpm;

  int modeBut = checkButton();
  if(modeBut < 0 || (modeBut > 0 && ++mode > 5)) mode = 0;
  clr();
  setCol(0,1<<mode);
  if(mode==0) showBigRPM(); else
  if(mode==1) showRPM_Max(); else
  if(mode==2) showCnt_RPM(); else
  if(mode==3) showCnt_Time(); else
  if(mode==4) showBigVal(); else
  if(mode==5) showBigCnt();
  refreshAll();
}
// =======================================================================
// dispCnt     - number of revolutions
// rpm         - current RPM
// maxrpm      - max RPM
// dispRotTime - time

void showCnt_Time()
{
  showNumber(dispCnt/3,4,1,4,small3x7,0);
  showNumber(dispRotTime,4,17,4,small3x7,0);
}
// =======================================================================
void showCnt_RPM()
{
  showNumber(rpm/3,4,1,4,small3x7,0);
  showNumber(dispCnt/3,4,17,4,small3x7,0);
}
// =======================================================================
void showRPM_Max()
{
  showNumber(rpm/3,4,1,4,small3x7,0);
  showNumber(maxrpm/3,4,17,4,small3x7,0);
}
// =======================================================================
void showBigRPM()
{
  showNumber(rpm/3,4,2,6,dig5x8rn,0);
}
// =======================================================================
void showBigCnt()
{
  showNumber(cnt,4,2,6,dig5x8rn,0);
}
// =======================================================================
void showBigVal()
{
  showNumber(irVal,4,2,6,dig5x8rn,200);
}
// =======================================================================
unsigned long lastTime=0;
int lastVal;
void showNumber(int val, int numDigits, int xPos, int wd, const uint8_t *font, unsigned long valDelay)
{
  if(curTime-lastTime>=valDelay) {
    lastVal = val;
    lastTime = curTime;
  }
  snprintf(txt,9,"%05u",lastVal); // 00000-99999
  int leading = 1;
  for(int i=0; i<numDigits; i++)
    if(txt[i-numDigits+5]!='0' || !leading || i==numDigits-1) { showDigit(txt[i-numDigits+5]-0x20, xPos+wd*i, font); leading = 0; }
}
// =======================================================================
int dx=0; // actually not used here
int dy=0;
void showDigit(char ch, int col, const uint8_t *data)
{
  if (dy < -8 | dy > 8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if (col + i >= 0 && col + i < 8 * NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if (!dy) scr[col + i] = v; else scr[col + i] |= dy > 0 ? v >> dy : v << -dy;
    }
}

// =======================================================================

void setCol(int col, byte v)
{
  if (dy < -8 | dy > 8) return;
  col += dx;
  if (col >= 0 && col < 8 * NUM_MAX)
      if (!dy) scr[col] = v; else scr[col] |= dy > 0 ? v >> dy : v << -dy;
}

// =======================================================================


