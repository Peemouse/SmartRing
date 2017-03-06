/*Neopixel Battery gauge (made for Neopixel ring)
 
Clement Le Priol - February 2017

Features :
- Displays the battery capacity like a gauge from full ring to empty ring
- The position of the gauge blinks smoothly and slowly
- If charging, the ring switches in blue gradient
- In case of very low battery, the whoe ring blinks in red
- Set the brightness according to the ambiant luminosity (needs a photoresistor)

TESTING : some input or variables have been created for testing. Comment/Modify/Delete if needed.
 */

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

//Hardware configuration
#define PIN 6 //Data out for WS2812 communication
#define NUM_LEDS 12 //number of LEDS in ring/strip/stick
#define BRIGHTNESS 50 //Set the brightness of the ring/strip/stick

const int lumResistorPin= A0; //Luminosity sensor for adjusting the brightness

Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

uint8_t lowBatLevel = 10; //battery level for triggering low battery animation (whole ring/strip/stick blinking in red)

//Used for testing
uint8_t capacity=60; //set the capacity (0-100%)
const uint8_t chargePin = 7; // pin for charge state (for testing)

uint8_t ringArray [NUM_LEDS+1];

void setup() {
  Serial.begin(9600);
  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.show(); // Initialize all pixels to 'off'
  //Starting animation
  for(int i=0; i<=ring.numPixels();i++) {
    ring.setPixelColor(i, 0, 45, 255);
    ring.show();
    delay(100);
  }

  //Calculation of the capacity array for choosing how many leds to display
  uint8_t nbLeds=ring.numPixels();
  uint8_t capStep=100/nbLeds;

  for (uint8_t i=0; i<=nbLeds; i++) {
    ringArray[i]=capStep*i;
  }
  
  pinMode(chargePin, INPUT_PULLUP); //(TEST) set the charge state pin as input with pullup resistor
}

void loop() {

//TO DEBUG
if (Serial.available()) {
  capacity=Serial.read()*10;
  Serial.println(capacity,DEC);
}

boolean charge = digitalRead(chargePin); //(TEST)

int lumResistor = analogRead(lumResistorPin);
uint8_t lum = map(lumResistor, 0, 1023, 0, 190);
//Serial.print("lumResistor="); //Used for calibration
//Serial.println(lumResistor);
lum=constrain(lum,15,190); //Sets a minimum to avoid having weird colors due to a too weak brightness
ring.setBrightness(lum);
//Serial.print("lum="); //Used for calibration
//Serial.println(lum);

//Select the last pixel showed and blinking on the ring/strip/stick
uint8_t lastPixel=selectLastPixel(capacity);

//Calculation of the ring color regarding the capacity
uint8_t redVal, greenVal, blueVal;
/*First pattern : all the ring in the same color (graduated regarding the capacity)
if (charge ==false) {
  if (capacity<=10) {
     redVal = 255;
     greenVal = 0;
  }
  else {
    greenVal = 255 * float(capacity/100.0);
    redVal = 255 - greenVal;
    blueVal=0;
  }
}
else {
  redVal = 0;
  greenVal = 0;
  blueVal = 255;
}

//Colore tous les pixels avec la couleur calculée
for(uint8_t i=ring.numPixels();i>=lastPixel;i--) {
  ring.setPixelColor(i, redVal, greenVal, blueVal);
  ring.show();
  delay(100);
} */

//Second pattern : the ring color is graduated whatever the capacity
if (charge ==false) {
  if (capacity<=lowBatLevel) {
    for(uint8_t i=ring.numPixels();i>lastPixel;i--) {
      ring.setPixelColor(i-1,255,0,0);
      ring.show();
      delay(100);
    }
  }
  else {
    for(uint8_t i=ring.numPixels();i>lastPixel;i--) {
      uint8_t r=255/ring.numPixels()*i;
      uint8_t g=255-r;
      uint8_t b=0;
      ring.setPixelColor(i-1,r,g,b);
      ring.show();
      delay(100);
    }
  }
}
else {
    for(uint8_t i=ring.numPixels();i>lastPixel;i--) {
      uint8_t r=0;
      uint8_t g=255-(255/ring.numPixels()*i);
      uint8_t b=255/ring.numPixels()*i;
      ring.setPixelColor(i-1,r,g,b);
      ring.show();
      delay(100);
    }
}

//Switch off the pixels after the last selected
for(uint8_t i=lastPixel;i>0;i--) {
  ring.setPixelColor(i-1, 0,0,0);
  ring.show();
  delay(100);
}

//Call the fade function of the last pixel if not in low battery state
if (capacity>lowBatLevel || charge){
  fade(lastPixel,8);
}
else {
  delay(200);
  for (uint8_t loops=0; loops<4;loops++) {
    for(uint8_t i=ring.numPixels(); i>0; i--) {
      ring.setPixelColor(i-1, 0, 0, 0);
    }
    ring.show();
    delay(300);
    for(uint8_t i=ring.numPixels(); i>0; i--) {
      ring.setPixelColor(i-1, 255, 0, 0);
    }
    ring.show();
    delay(100);
  }
  return;
}

  delay(2000);
}

//Selection of the last pixel according to the capacity
uint8_t selectLastPixel (uint8_t value) {
  uint8_t nbLeds = ring.numPixels();
  //At least 1 led displayed even if capacity = 0
    for(uint8_t i=1; i<=nbLeds;i++) {
      if (value <= ringArray[i]){
        return (nbLeds - i);
      }
    }
}

void fade (uint8_t pixel, uint16_t wait) {
  float fadeMax = 100.0;
  uint8_t fadeVal = 100;
  uint32_t wheelVal;
  uint8_t redValFade, greenValFade, blueValFade;
  wheelVal = ring.getPixelColor(pixel);
  
  for(uint8_t i=0; i<200 ; i++) { 
    if (i<100) {
      fadeVal--;
    }
    else {
      fadeVal++;
    }
    
    redValFade = red(wheelVal) * float(fadeVal/fadeMax);
    greenValFade = green(wheelVal) * float(fadeVal/fadeMax);
    blueValFade = blue(wheelVal) * float(fadeVal/fadeMax);

    ring.setPixelColor(pixel, greenValFade, redValFade, blueValFade);

    ring.show();
    delay(wait);
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
