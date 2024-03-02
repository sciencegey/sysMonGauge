// MIT License
//
// Copyright (c) 2024 sciencegey
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define DEBUG false  //set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial1

#define CALIBRATE false // set to true to calibrate the dials

#include <Arduino.h>
#include <Wire.h>
#include <TM1637Display.h>

#include <Adafruit_NeoPixel.h>
#define PIN 22
#define NUMPIXELS 144
Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define BASECOL strip.Color(32, 32, 8) // Warm white
#define WARNCOL strip.Color(255, 255, 0)  // Yellow
#define ERRORCOL strip.Color(255, 0, 0) // Red

// Maps out which pixels correspond to which gauge (start, end)
const uint8_t pixelMap[][2] = {{0,9},{28,37},{19,28},{47,56},{37,47},{9,19}};
 
const uint8_t BUFFER_SIZE = 31;
// cpuUsage, ramUsage, diskRead, diskWrite, networkSent, networkRecv, diskUsage, systemUptime
char serBuf[BUFFER_SIZE];
char serBufLast[BUFFER_SIZE];
char diskUsage[16];

// Pins for the gauges
#define cpuUsagePin 2
#define ramUsagePin 3
#define diskReadPin  4
#define diskWritePin 5
#define networkSentPin 6
#define networkRecvPin 7

// Pins for the 7 segment displays
#define LED0_CLK 8
#define LED0_DIO 9
#define LED1_CLK 10
#define LED1_DIO 11

// Pins used for the rotary switch
const uint8_t swPins[] = {18, 19, 20, 21};
// 0 = clock, 1 = network send/receive, 2 = disk read/write, 3 = cpu speed, 4 = disk usage
uint8_t swPos = 0;
// Pins used for the LED indicators
const uint8_t ledPins[] = {12, 13, 14, 15, 16};

TM1637Display display0(LED0_CLK,LED0_DIO);
TM1637Display display1(LED1_CLK,LED1_DIO);

uint32_t systemUptimeBuf = 0;
uint32_t systemUptime = 0;
uint32_t diskReadRaw = 0;
uint32_t diskWriteRaw = 0;
uint32_t networkSentRaw = 0;
uint32_t networkRecvRaw = 0;
uint16_t cpuCurrentHz = 0;
uint16_t cpuMaxHz = 0;

// How many milliseconds between the page changing
#define refresh 2000

// What page we're on for pages that scroll
uint8_t page = 0;

uint8_t timeBuffer[4] = {0,0,0,0};
uint16_t displayBuffer = 0;

uint8_t numDisks = 0;

uint32_t then = millis();

void setup() {
  Serial.begin(115200);
  DEBUG_SERIAL.setTX(0);
  DEBUG_SERIAL.setRX(1);
  DEBUG_SERIAL.begin(115200);
  DEBUG_SERIAL.println("Starting!");
  pinMode(LED_BUILTIN, OUTPUT);

  // Setup the pins for each gauge as an output
  pinMode(cpuUsagePin, OUTPUT);
  pinMode(ramUsagePin, OUTPUT);
  pinMode(diskReadPin, OUTPUT);
  pinMode(diskWritePin, OUTPUT);
  pinMode(networkSentPin, OUTPUT);
  pinMode(networkRecvPin, OUTPUT);

  // Setup the pins for the input switch
  for(uint8_t i=0;i<(sizeof(swPins)/sizeof(swPins[0]));i++){
    pinMode(swPins[i], INPUT_PULLUP);
  }
  // Setup the pins for the LED indicators
  for(uint8_t i=0;i<(sizeof(ledPins)/sizeof(ledPins[0]));i++){
    pinMode(ledPins[i], OUTPUT);
  }

  // Brightness between 0 and 255
  display0.setBrightness(128);
  display1.setBrightness(128);
  display0.clear();
  display1.clear();

  strip.begin();
  strip.show();
  // Brightness between 0 and 255
  strip.setBrightness(50);
  // Set each pixel with a small delay so it ~looks cool~
  for(int i=0; i<NUMPIXELS; i++) {
    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    // Here we're using a colour that's equivilent to a old light
    strip.setPixelColor(i, BASECOL);
    strip.show();   // Send the updated pixel colors to the hardware.
    delay(15); // Pause before next pass through loop
  }

  // Copy the new data into the last buffer so we can compare it to the next time around.
  memcpy(serBufLast, serBuf, sizeof(serBuf[0])*BUFFER_SIZE);

  DEBUG_SERIAL.println("Ready!");

  while (CALIBRATE) {
    // To calibrate the dials, make sure the dials are zeroed, then set CALIBRATE to true
    // Then adjust the variable resistors so the needles line up with 100/1000
    // Then set calibrate to false and reupload the sketch

    for(int i=0; i<NUMPIXELS; i++) {
      // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
      // Here we're using a colour that's equivilent to a old light
      strip.setPixelColor(i, strip.Color(0, 0, 32));
      strip.show();   // Send the updated pixel colors to the hardware.
    }
    
    DEBUG_SERIAL.println("Calibrating dials");

    analogWrite(cpuUsagePin, 255);
    analogWrite(ramUsagePin, 255);
    analogWrite(diskReadPin, 255);
    analogWrite(diskWritePin, 255);
    analogWrite(networkSentPin, 255);
    analogWrite(networkRecvPin, 255);
  }
}

void loop() {
  // Get the system stats over serial
  if(Serial.available() > 0) {
    Serial.readBytes(serBuf, BUFFER_SIZE);

    // DEBUG_SERIAL.print("Got data: ");
    // for(int i = 0; i < BUFFER_SIZE; i++) {
    //   DEBUG_SERIAL.print(serBuf[i], DEC);
    //   DEBUG_SERIAL.print(" ");
    // }
    // DEBUG_SERIAL.println(swPos);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(5);
    digitalWrite(LED_BUILTIN, LOW);
    delay(5);
  }

  if(memcmp(serBufLast, serBuf, sizeof(serBufLast[0]) * BUFFER_SIZE) != 0) {
    // Only update the displays if we have any new data
    // Copy the new data into the last buffer so we can compare it to the next time around.
    memcpy(serBufLast, serBuf, sizeof(serBuf[0])*BUFFER_SIZE);
    // DEBUG_SERIAL.println("New data!");


    // TODO : Smooth transition between values
    analogWrite(cpuUsagePin, int(serBuf[0]));
    analogWrite(ramUsagePin, int(serBuf[1]));
    analogWrite(diskReadPin, int(serBuf[2]));
    analogWrite(diskWritePin, int(serBuf[3]));
    analogWrite(networkSentPin, int(serBuf[4]));
    analogWrite(networkRecvPin, int(serBuf[5]));

    DEBUG_SERIAL.print("CPU: ");
    DEBUG_SERIAL.print(serBuf[0], DEC);
    DEBUG_SERIAL.print(" RAM: ");
    DEBUG_SERIAL.print(serBuf[1], DEC);
    DEBUG_SERIAL.print(" DISK READ: ");
    DEBUG_SERIAL.print(serBuf[2], DEC);
    DEBUG_SERIAL.print(" DISK WRITE: ");
    DEBUG_SERIAL.print(serBuf[3], DEC);
    DEBUG_SERIAL.print(" NET SEND: ");
    DEBUG_SERIAL.print(serBuf[4], DEC);
    DEBUG_SERIAL.print(" NET RECV: ");
    DEBUG_SERIAL.println(serBuf[5], DEC);


    // analogWrite(cpuUsagePin, 255);
    // analogWrite(ramUsagePin, 255);
    // analogWrite(diskReadPin, 255);
    // analogWrite(diskWritePin, 255);
    // analogWrite(networkSentPin, 255);
    // analogWrite(networkRecvPin, 255);

    for(uint8_t i=0; i<=6; i++){
      if(int(serBuf[i]) >=204){
        // Critical!
        for(uint8_t j=pixelMap[i][0]; j<pixelMap[i][1]; j++){
          strip.setPixelColor(j, ERRORCOL);
        }
      } else if(int(serBuf[i]) >=153){
        // Warning!
        for(uint8_t j=pixelMap[i][0]; j<pixelMap[i][1]; j++){
          strip.setPixelColor(j, WARNCOL);
        }
      } else {
        // Normal :)
        for(uint8_t j=pixelMap[i][0]; j<pixelMap[i][1]; j++){
          strip.setPixelColor(j, BASECOL);
        }
      }
    }
    strip.show();   // Send the updated pixel colors to the hardware.

    // The system time is a 32 bit "long", so it comes in in 4 byte chunks
    systemUptimeBuf = serBuf[10] << 24 | serBuf[9] << 16 | serBuf[8] << 8 | serBuf[7];

    // DEBUG_SERIAL.print("System uptime: ");
    // DEBUG_SERIAL.print(serBuf[10], DEC);
    // DEBUG_SERIAL.print(serBuf[9], DEC);
    // DEBUG_SERIAL.print(serBuf[8], DEC);
    // DEBUG_SERIAL.println(serBuf[7], DEC);

    // Same for these ones
    diskReadRaw = serBuf[14] << 24 | serBuf[13] << 16 | serBuf[12] << 8 | serBuf[11];
    diskWriteRaw = serBuf[18] << 24 | serBuf[17] << 16 | serBuf[16] << 8 | serBuf[15];
    networkSentRaw = serBuf[22] << 24 | serBuf[21] << 16 | serBuf[20] << 8 | serBuf[19];
    networkRecvRaw = serBuf[26] << 24 | serBuf[25] << 16 | serBuf[24] << 8 | serBuf[23];

    // But these are "short"s, so they're only 2 bytes
    cpuCurrentHz = serBuf[28] << 8 | serBuf[27];
    cpuMaxHz = serBuf[30] << 8 | serBuf[29];

    // Arrays start at zero, but disks don't ;)
    numDisks = int(serBuf[31])-1;

    // Get the current position of the selector switch
    getSwitchPos(swPos);
    //DEBUG_SERIAL.println("Switch Position: " + String(swPos));

    // Set the indicators for whichever page we're on
    for(uint8_t i=0;i<(sizeof(ledPins)/sizeof(ledPins[0]));i++){
      digitalWrite(ledPins[i], LOW);
    }
    digitalWrite(ledPins[swPos], HIGH);

    // Get the current millis
    uint32_t now = millis();

    if (swPos == 0){
      // System Uptime
      if(systemUptime < systemUptimeBuf){
        // Only update the display if the system time has actually changed (and systemtime is selected)
        //DEBUG_SERIAL.println("It's time to update");
        systemUptime = systemUptimeBuf;
        secondsToHMS(systemUptime, timeBuffer);

        // Display the minutes and seconds
        displayBuffer = 100*timeBuffer[2]+timeBuffer[3];
        //DEBUG_SERIAL.println("Minutes and seconds: " + String(displayBuffer));
        display1.showNumberDecEx(displayBuffer, 0b01000000, true);  // Print with the dots in the middle

        // Then display the days and hours
        displayBuffer = 100*timeBuffer[0]+timeBuffer[1];
        //DEBUG_SERIAL.println("Days and hours: " + String(displayBuffer));
        display0.showNumberDecEx(displayBuffer, 0b01000000, true);  // Print with the dots in the middle
      }
    } else if (swPos == 1){
      // Network sent/received
      // Show it in megabits a second
      display0.showNumberDec(networkSentRaw, true);
      display1.showNumberDec(networkRecvRaw, true);
    } else if (swPos == 2){
      // Disk read/write
      // Show it in megabytes a second
      display0.showNumberDec(diskReadRaw, true);
      display1.showNumberDec(diskWriteRaw, true);
    } else if (swPos == 3){
      // CPU speed
      display0.showNumberDec(cpuCurrentHz, true);
      display1.showNumberDec(cpuMaxHz, true);
    } else if (swPos == 4){
      // // Disk space usage
      // // Cycle through the pages
      // if(now - then >= refresh){
      //   then = now;
      //   page++;
      //   if(page > numDisks){
      //     page = 0;
      //   }
      // }
      // // Drive indicator doesn't need leading zeros
      // display0.showNumberDec(page, false);
      // // Convert the 0-255 value into a 0-100 (percentage) value
      // display1.showNumberDec(int(map(serBuf[31+page],0,255,0,100)), false);
    }
  }
}

void secondsToHMS(const uint32_t seconds, uint8_t *outBuf) {
  uint32_t t = seconds;
  uint8_t s = 0;
  uint8_t m = 0;
  uint8_t h = 0;
  uint8_t d = 0;
  
  s = t % 60; // First calculate seconds by moduloing by 60 (seconds in a minute)

  t = (t - s)/60; // Then subtract the seconds we've already accounted for, then divide by 60
  m = t % 60; // And do the same to calculate minutes (60 minutes in an hour)

  t = (t - m)/60; // Do the same as before
  h = t % 24; // But this time modulo by 24 (hours in a day)

  t = (t - h)/24; // And finally we're left with just the days
  d = t;

  // DEBUG_SERIAL.print("Days: ");
  // DEBUG_SERIAL.print(d);
  // DEBUG_SERIAL.print(" Hours: ");
  // DEBUG_SERIAL.print(h);
  // DEBUG_SERIAL.print(" Minutes: ");
  // DEBUG_SERIAL.print(m);
  // DEBUG_SERIAL.print(" Seconds: ");
  // DEBUG_SERIAL.println(s);
  outBuf[0] = d;
  outBuf[1] = h;
  outBuf[2] = m;
  outBuf[3] = s;
}

void getSwitchPos(uint8_t& swPos){
  // If none of the pins are activated, then the switch is "off" (0)
  swPos = 0;

  // Get the input of each pin, and return when we hit the first pin to be set
  for(uint8_t i=0;i<4;i++){
    if(digitalRead(swPins[i]) == LOW){
      swPos = i+1;
      return;
    }
  }
  return;
}