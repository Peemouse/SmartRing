/*SmartRing - Monitoring VESC data and manage rear/brake light using RGB leds 

Clement Le Priol - February 2018
v1.0

Github : https://github.com/Peemouse/SmartRing

Features :
- Displays the battery, speed and motor temperature through page to scroll (by long pushing the remote button)
- In case of very low battery, the whole ring blinks in red
- Set the brightness according to the ambiant luminosity (needs a photoresistor)
- Switch rear and front light through PPM channel on GT2B receiver (could probably work with other remotes)
- Activate brake rearlight while braking

 */

#include "VescUart.h"
#include "datatypes.h"
#include "LiPoCheck.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

//Hardware configuration
#define WS2812PIN 2 //Data out for WS2812 communication
#define BRIGHTNESS 255 //Set the initial brightness of the ring/strip/stick
const int builtinLedPin =  13;
const int ppmPin = 3;
const int pushBtnPin = 8;
const int lumResistorPin = A0; //Luminosity sensor for adjusting the brightness

//Software Configuration
const float BrakeCurDeadband = 1.0; // Brake current deadband for switching rearlight on (value must be strictly lower than 0.01 A)
const long commVescTimeout = 2000; // VESC communication timeout (im ms)

//Working variables
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS_TOTAL, WS2812PIN, NEO_GRB + NEO_KHZ800);
struct bldcMeasure measuredValues;
float voltageIdle;
uint8_t ringArray [NUM_LEDS_RING+1];
bool toogle, commVescSt = false;
uint8_t chenilCount = 0;
uint8_t fadeVal = 100;
int capacity=0;
uint8_t lastPixel=0;
uint8_t page;
int i,j,k;
bool initAnim=true;
int lumResistor;
bool lightSt, mem; //Status bits
bool lightAuto;

//Timers
unsigned long timer1=0;
unsigned long timer2=0;
unsigned long timer3=0;
unsigned long timer4=0;
unsigned long timerLongPush=0;
unsigned long timerCommVesc=0;

void setup() {
  pinMode(ppmPin, INPUT);
  pinMode(pushBtnPin, INPUT_PULLUP);
  pinMode(builtinLedPin, OUTPUT);

  Serial.begin(9600);

  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.show(); // Initialize all pixels to 'off'

  //Startup animation
  for(uint8_t i=0; i<NUM_LEDS_RING;i++) {
    for(uint8_t j=0; j<=100; j+=10) {
    //ring
      uint8_t g = uint8_t(45.0 * float(j) /100.0);
      uint8_t b = uint8_t(255.0 * float(j) /100.0);
      ring.setPixelColor(i, 0, g, b);

    //Stick
    if (i<4){
      uint8_t r = uint8_t(255.0 * float(j) /100.0);
      ring.setPixelColor(NUM_LEDS_RING+4+i, r, 0, 0);
      ring.setPixelColor(NUM_LEDS_RING+3-i, r, 0, 0);
    }
    else if (i>=8 && i<12) {
      uint8_t r = uint8_t(255.0 * (100.0 - float(j))/100.0);
      ring.setPixelColor(NUM_LEDS_RING+4+(i-8), r, 0, 0);
      ring.setPixelColor(NUM_LEDS_RING+3-(i-8), r, 0, 0);
    }
    ring.show();
    delay(12);
    }
  }

  //Calculation of the capacity array for choosing how many leds to display

  uint8_t capStep=100/NUM_LEDS_RING;

  for (uint8_t i=0; i<=NUM_LEDS_RING; i++) {
    ringArray[i]=capStep*i;
  }

  initBattArray(); //Select the capacity array corresponding to the battery chemistry (see BATT_TYPE in config.h)
  page = 0;
  delay(800); //More time to let the VESC completely start
  timerLongPush=millis();
}

void loop() {

  bool shortPush, pushBtn = false;
  bool disablePPM = false;
  bool longPush = false;
  uint32_t wheelVal;
  uint8_t redValFade, greenValFade, blueValFade, dynFrontLightPwr;
  uint8_t selectLastPixel(uint8_t value);
  uint8_t red(uint32_t c);
  uint8_t green(uint32_t c);
  uint8_t blue(uint32_t c);

  //Automatic brightness adjustment (regarding ambiant luminosity)

  lumResistor += (analogRead(lumResistorPin) - lumResistor) * 0.2; //filtering the analog value
  //Serial.println(lumResistor);

  uint8_t lum = map(lumResistor, 0, 1023, 0, 255);
  lum=constrain(lum,30,255); //Sets a minimum to avoid having weird colors due to a too weak brightness

  //Read PPM from receiver
  int ppm = pulseIn(ppmPin, HIGH, 50000); //PWM from GT2B receiver CH3 : about 1000 microseconds when off, 2000 when on.

  //Read wired momentary push button state
  pushBtn = !digitalRead(pushBtnPin);

//  //Read PPM from CH3 channel (ON/OFF) of receiver for activating light
//  if (ppm > 1700) {
//    lightSt = true;
//    digitalWrite(builtinLedPin, HIGH);
//  }
//  else if (ppm < 1300) {
//    lightSt = false;
//    digitalWrite(builtinLedPin, LOW);
//  }
//  Serial.print("pushbtn ");
//  Serial.println(pushBtn);
  
  //Read PPM from CH1 channel (momentary) of receiver and push button
  if ((ppm < 1100) || (ppm > 4000)) { //issue on the PPM input or remote disconnected
    disablePPM = true;
  }

  if (!disablePPM && (ppm < 1500) || pushBtn) {
    if (!mem) {
      timerLongPush = millis();
      mem = true;
    }
  }
  else {
    if (millis() - timerLongPush > 1500) { //Check for long push
      longPush = true;
    }
    else if (mem == true) { // short push
      shortPush = true;
    }
    else {
      shortPush = false;
    }
      timerLongPush = millis();
      mem = false;
  }

//Read data and check communication status with VESC
  if (VescUartGetValue(measuredValues)) {
    commVescSt = true;
    timerCommVesc = millis();
  }
  else if (millis() - timerCommVesc > commVescTimeout) {
    commVescSt = false;
  }

  if (commVescSt) { //VESC communication is alive

  float instantSpeed;
  uint8_t speedRatio;
  bool brakeSt = false;

  digitalWrite(builtinLedPin, HIGH); //

  if (initAnim) {
    for (uint8_t z=0; z<NUM_LEDS_RING;z++) {
      ring.setPixelColor(z,0,0,0);
      ring.show();
      delay(100); 
    }
    initAnim=false;
    page = 0;
    timerLongPush = millis();
  }

  //Battery capacity calculation

  if (measuredValues.avgInputCurrent < 2.5) {
    if (millis()-timer3 > 1000) {
      voltageIdle = measuredValues.inpVoltage;
      timer3=millis();
    }
  }
  capacity = CapCheckPerc(voltageIdle, CELLS_NUMBER);

  //Select the last pixel showed and blinking on the ring/strip/stick
  lastPixel=selectLastPixel(capacity);

  //Calculate relative speed ratio (0-100% = 0 km/h to MAX_SPEED in config.h)
  instantSpeed = measuredValues.rpm/14.0*GEAR_RATIO*WHEEL_DIAM/5305.16; //exact formula : erpm/motorPoles/60*motorPulley/drivePulley*wheelDiam/1000*3.14159*3.6
  instantSpeed = constrain (instantSpeed, 0.0, MAX_SPEED);
  speedRatio = (uint8_t)(instantSpeed / MAX_SPEED * 100.0); //Calculate how many pixels to light up on each half ring
  
  if (capacity<=LOW_BAT_LEVEL) { //Battery low
    if (shortPush) {
        lightSt = !lightSt;
        lightAuto = false;
      }
    if(millis()-timer1 > 200){
      toogle=!toogle;
      if (toogle) {
        for (int z;z<NUM_LEDS_RING;z++){
        ring.setPixelColor(z,255,0,0);
        }
      }
      else {
        ring.clear();
      }
      timer1=millis();
    }
  }
  else { //Not battery low

    uint8_t nbPixel,tempPrct;

    if (longPush) { //Change the "page" (aka the mode displayed on the ring)
      page++;
      if (page > 2) {
        page=0;
      }
      longPush = false;
    }

    switch (page){

    case 0 : //default menu : battery gauge
      if (shortPush) {
        lightSt = !lightSt;
        lightAuto = false;
      }
      for(uint8_t i=NUM_LEDS_RING;i>lastPixel;i--) {
        uint8_t r=255/NUM_LEDS_RING*i;
        uint8_t g=255-r;
        uint8_t b=0;
        ring.setPixelColor(i-1,r,g,b);
      }

      //Switch off the pixels after the last selected
      for(uint8_t i=lastPixel;i>0;i--) {
        ring.setPixelColor(i-1, 0,0,0);
      }

      //Fade in & out the last pixel if not in low battery state
      if ((micros()-timer2)>=1000) {

        wheelVal = ring.getPixelColor(lastPixel);

        if (j<50) {
          fadeVal=fadeVal-2;
        }
        else {
          fadeVal=fadeVal+2;
        }
        j++;
        if (j>100){j=0;}

        redValFade = red(wheelVal) * float(fadeVal/100.0);
        greenValFade = green(wheelVal) * float(fadeVal/100.0);
        blueValFade = blue(wheelVal) * float(fadeVal/100.0);

        ring.setPixelColor(lastPixel, greenValFade, redValFade, blueValFade);

        timer2=micros();
      }
      break;

    case 1 : //Speed indicator : vertical symetry gauge from bottom to top (0 km/h to MAX_SPEED - set in config.h)
      if (shortPush) {
        lightSt = !lightSt;
        lightAuto = false;
      }

      nbPixel = (uint8_t)(NUM_LEDS_RING / 2 * speedRatio / 100.0);
      redValFade = (uint8_t)(speedRatio*255.0/100.0);
      greenValFade = 0;
      blueValFade = 255;
      ring.clear();
      for(uint8_t i=0; i<=nbPixel; i++) {
        ring.setPixelColor(NUM_LEDS_RING / 2 - 1 - i, redValFade, greenValFade, blueValFade);
        ring.setPixelColor(NUM_LEDS_RING / 2 + i, redValFade, greenValFade, blueValFade);
      }

      break;

    case 2 : //Temperature indicator (empty ring = 20°C, full ring = 100°C), counterclockwise from blue to red
      if (shortPush) {
        lightSt = !lightSt;
        lightAuto = false;
      }
      ring.clear();
      if (measuredValues.tempMotor > 0.0) { //fulfill the whole ring in light white if ok, orange if the 80°C limit is reached (VESC is regulating amps)
        if (measuredValues.tempMotor > 80.0) {
          for(uint8_t i = toogle ? 1 : 0 ; i<NUM_LEDS_RING; i+=2) {
            ring.setPixelColor(i, 255, 170, 0);
            toogle=!toogle;
          }
        }
        else {
          for(uint8_t i = 0 ; i<NUM_LEDS_RING; i+=2) {
            ring.setPixelColor(i, 130, 130, 130);
          }
        }
      }
      else { //No motor temperature sensors or wire break
        for(uint8_t i = 0 ; i<NUM_LEDS_RING; i+=2) {
            ring.setPixelColor(i, 255, 0, 70);
        }
      }
      
      break;

    default:
      break;

    }
  }
  // Rearlight management

  if (measuredValues.avgMotorCurrent < (0.0 - BrakeCurDeadband)) {
    brakeSt=true;
  }
  else if (measuredValues.avgMotorCurrent > -0.01){
    brakeSt=false;
  }

  if (brakeSt) { //Braking : full rear light
    rearLightBrake();
  }
  else if (!brakeSt && lightSt) { //Not Braking but light activated : medium rear light
    rearLightNoBrake();
  }
  else { //Not Braking, no light activated : no rear light
    rearLightOff();
  }

  }
  else { //If failed to communicate with VESC
    initAnim=true; //This will trigger the animation transition once the VESC communication will recover
    digitalWrite(builtinLedPin, LOW);

    if (shortPush){
      lightSt = !lightSt;
    }

  //Pattern : chase
      ring.clear();

      if (chenilCount==(NUM_LEDS_RING-2)) {
        ring.setPixelColor(10, 75, 0, 130);
        ring.setPixelColor(11, 75, 0, 130);
        ring.setPixelColor(0, 75, 0, 130);
      }
     else if (chenilCount==(NUM_LEDS_RING-1)) {
        ring.setPixelColor(11, 75, 0, 130);
        ring.setPixelColor(0, 75, 0, 130);
        ring.setPixelColor(1, 75, 0, 130);
      }
     else {
      for (uint8_t k=0; k<3; k++) {
          ring.setPixelColor(chenilCount+k,75, 0, 130);
        }
     }

    if (lightSt) { //Light activated but no brakes : medium rear light
      rearLightNoBrake();
    }
    else {
      rearLightOff();
    }

    if (millis() - timer4 >= 250) {
      chenilCount++;
      timer4 = millis();
    }
    if (chenilCount==NUM_LEDS_RING){
      chenilCount=0;
    }

  }
  adaptBrightness(lum); //This void because it's not recommended to use set.Brightness() in the loop, only once in setup.
  ring.show();

  delay(50);
}

//Selection of the last pixel according to the capacity
uint8_t selectLastPixel (uint8_t value) {

  //At least 1 led displayed even if capacity = 0
    for(uint8_t z=1; z<=NUM_LEDS_RING;z++) {
      if (value <= ringArray[z]){
        return (NUM_LEDS_RING - z);
      }
    }
}

void adaptBrightness(uint8_t value) {

  for (uint8_t z=0;z<NUM_LEDS_RING;z++) {
    uint8_t redVal, greenVal, blueVal;
    uint32_t currentColor;
    currentColor = ring.getPixelColor(z);

    redVal = red(currentColor) * float(value/255.0);
    greenVal = green(currentColor) * float(value/255.0);
    blueVal = blue(currentColor) * float(value/255.0);

    ring.setPixelColor(z, greenVal, redVal, blueVal);
  }
}

void rearLightOff() {
  for (uint8_t z=NUM_LEDS_RING; z<NUM_LEDS_TOTAL; z++){
      ring.setPixelColor(z,0, 0, 0); //Set red medium brightness
  }
}

void rearLightNoBrake() {
  for (uint8_t z=NUM_LEDS_RING; z<NUM_LEDS_TOTAL; z++){
      ring.setPixelColor(z,0, 0, 0); //Set red medium brightness
  }
  for (uint8_t z=0; z<2; z++){
      ring.setPixelColor(NUM_LEDS_RING+z,255, 0, 0); //Set red medium brightness
      ring.setPixelColor(NUM_LEDS_RING+NUM_LEDS_REARLIGHT-1-z,255, 0, 0); //Set red medium brightness
  }
}

void rearLightBrake() {
  for (uint8_t z=NUM_LEDS_RING; z<NUM_LEDS_TOTAL; z++){
      ring.setPixelColor(z,255, 0, 0); //Set red medium brightness
  }
}

uint8_t red(uint32_t c) {
  return (c >> 8);
}
uint8_t green(uint32_t c) {
  return (c >> 16);
}
uint8_t blue(uint32_t c) {
  return (c);
}
