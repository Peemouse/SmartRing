#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 6
#define NUM_LEDS 12
#define BRIGHTNESS 15

Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);
int capacite=80;

const int btnPin =  7;      // the number of the LED pin

void setup() {
  Serial.begin(9600);
  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.show(); // Initialize all pixels to 'off'
  //Animation de début
  for(uint8_t i=0; i<(ring.numPixels());i++) {
    ring.setPixelColor(i, 0, 45, 255);
    ring.show();
    delay(100);
  }

  pinMode(btnPin, INPUT_PULLUP);
}

void loop() {

if (Serial.available()) {
  capacite=Serial.read()*10;
  Serial.println(capacite,DEC);
}

boolean charge = digitalRead(btnPin);

//Détermine le pixel de position de jauge
uint8_t lastPixel=selectLastPixel(capacite);

//Calcul de la couleur du ring en fonction de la capacite
uint8_t redVal, greenVal, blueVal; /*
if (charge ==false) {
  if (capacite<=10) {
     redVal = 255;
     greenVal = 0;
  }
  else {
    greenVal = 255 * float(capacite/100.0);
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

//test degrade
if (charge ==false) {
  if (capacite<=10) {
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

//Eteint les derniers pixels bleus
for(uint8_t i=lastPixel;i>0;i--) {
  ring.setPixelColor(i-1, 0,0,0);
  ring.show();
  delay(100);
}


/*Fade pixel de position de jauge
int fadeVal = 100;
for (uint8_t i=0; i < 200; i++) {
  float fadeMax = 100.0;
  int redValFade, greenValFade, blueValFade;

  //First loop, fade in!
  if (i<100){
    fadeVal--;
  }
  else {
    fadeVal++;
  }
  redValFade = redVal * float(fadeVal/fadeMax);
  greenValFade = greenVal * float(fadeVal/fadeMax);
  blueValFade = blueVal * float(fadeVal/fadeMax);

  ring.setPixelColor(lastPixel, redValFade, greenValFade, blueValFade );
  ring.show();
  delay(100);
} */

if (capacite>10 || charge){
  fade(lastPixel,10);
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

uint8_t selectLastPixel (uint8_t value) {
if (value>90){return 0;}
if (value<91 && capacite > 80){return 1;}
if (value<81 && capacite > 70){return 2;}
if (value<71 && capacite > 60){return 3;}
if (value<61 && capacite > 50){return 4;}
if (value<51 && capacite > 40){return 5;}
if (value<41 && capacite > 30){return 6;}
if (value<31 && capacite > 20){return 7;}
if (value<21 && capacite > 10){return 8;}
if (value<11){return 9;}
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
