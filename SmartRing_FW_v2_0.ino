

/*SmartRing - Monitoring VESC data and manage rear/brake light using RGB leds

Clement Le Priol - February 2019
FW v2.0

Github : https://github.com/Peemouse/SmartRing

Features :
- Displays the battery, speed and motor temperature through page to scroll (by long pushing the remote button)
- In case of very low battery, the whole ring blinks in red
- Set the brightness according to the ambiant luminosity (needs a photoresistor)
- Switch rear and front light through PPM channel on GT2B receiver (could probably work with other remotes)
- Activate brake rearlight while braking

 */
#include "HW_type.h"
#include "LiPoCheck.h"
#include "config.h"
#include "eeprom_config.h"

#include <VescUart.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

//Working variables
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LEDS_TOTAL, WS2812PIN, NEO_GRB + NEO_KHZ800);

float voltageIdle;
const float ratio = 1000/7.0*GEAR_RATIO*WHEEL_DIAM/19098.6; //Calculation of transmission ratio (ERPM -> km). Exact formula : erpm/motorPolesPair/60*motorPulley/drivePulley*wheelDiam/1000*3.14159;
uint8_t ringArray [NUM_LEDS_RING+1];
bool toogle, commVescSt = false;
bool toogleTurnLight = true;
uint8_t chenilCount = 0;
uint8_t fadeVal = 100;
uint8_t page = 0;
bool initAnim=true;
int lumResistor;
bool lightSt, mem; //Status bits
bool resetTry = false; //Reset try flag
bool autoResetTry = false;
uint8_t ringStartBit = 0;
uint8_t stickStartBit = NUM_LEDS_RING;
uint8_t headLightStartBit = NUM_LEDS_REARLIGHT + NUM_LEDS_RING;
uint8_t modeSelect,currentMode;
bool blinkLeds = false;
//bool EEPROMwriteAllowed = false; //used in case more conditions have to be taken into consideration for EEPROM writing
bool memoCutOffReached = false;
static bool battCharged = false;

uint16_t totalCapacityDischargedStartup = 0;
uint16_t tripCapacityDischarged = 0;
uint16_t capacityMemo = 0;
long tachoMemo = 0;

const uint32_t modesColor[5] = {0xFFFFFF, 0x00FF00, 0x808000, 0xFF0000, 0x0000FF}; //Modes : 0-Default-White / 1-Mode 1-Green / 2-Mode 2-Orange / 3-Mode 3-Red / 4-Bluetooth-Blue

//Timers
unsigned long timerToogleFault=0;
unsigned long timerToogleLowBatt=0;
unsigned long timerToogleMode=0;
unsigned long idleVoltageDelay=0;
unsigned long timerChenil=0;
unsigned long timerLongPush=0;
unsigned long timerCommVesc=0;
unsigned long timerLastCommAlive=0;
unsigned long modeMenuTimeout=0;
unsigned long EEPROMdelay=0;
unsigned long turnLightTimer=0;
unsigned long battUnchargedDelay=0;
unsigned long maintainFaultDelay=0;

VescUart UART;

void setup() {
  pinMode(PPMPIN, INPUT);
  pinMode(PUSHBTNPIN, INPUT_PULLUP);
  pinMode(BUILTINLEDPIN, OUTPUT);

  Serial.begin(BAUDRATE);

  while (!Serial) {;}

  /** Define which ports to use as UART */
  UART.setSerialPort(&Serial);

  ring.begin();
  ring.setBrightness(BRIGHTNESS);
  ring.show(); // Initialize all pixels to 'off'

#ifdef REARLIGHT_FIRST
  ringStartBit = NUM_LEDS_REARLIGHT;
  stickStartBit = 0;
#endif

  //Startup animation TODO new animation
  for(uint8_t i=0; i<NUM_LEDS_RING;i++) {
    for(uint8_t j=0; j<=100; j+=10) {
    //ring
      uint8_t g = uint8_t(45.0 * float(j) /100.0);
      uint8_t b = uint8_t(255.0 * float(j) /100.0);
      ring.setPixelColor(i + ringStartBit, 0, g, b);

    //Stick
    if (i<4){
      uint8_t r = uint8_t(255.0 * float(j) /100.0);
      ring.setPixelColor(4+i+stickStartBit, r, 0, 0);
      ring.setPixelColor(3-i+stickStartBit, r, 0, 0);
      ring.setPixelColor(12+i+stickStartBit, r, 0, 0);
      ring.setPixelColor(11-i+stickStartBit, r, 0, 0);
    }
    else if (i>=8 && i<12) {
      uint8_t r = uint8_t(255.0 * (100.0 - float(j))/100.0);
      ring.setPixelColor(4+(i-8)+stickStartBit, r, 0, 0);
      ring.setPixelColor(3-(i-8)+stickStartBit, r, 0, 0);
      ring.setPixelColor(12+(i-8)+stickStartBit, r, 0, 0);
      ring.setPixelColor(11-(i-8)+stickStartBit, r, 0, 0);
    }

    ring.show();
    delay(12);
    }
  }
  //State of Battery
  battCharged = getBattState();

  //Calculation of the capacity array for choosing how many leds to display
  uint8_t capStep=100/NUM_LEDS_RING;

  for (uint8_t i=0; i<=NUM_LEDS_RING; i++) {
    ringArray[i]=capStep*i;
  }

  initBattArray(); //Select the capacity array corresponding to the battery chemistry (see BATT_TYPE in config.h)

  //Retrieve consumed mAh from EEPROM
  totalCapacityDischargedStartup = getCoulombCounter();

  delay(1200); //More time to let the VESC completely start

  timerLongPush=millis(); //Initialize long button push timer
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

  lumResistor += (analogRead(LUMRESISTORPIN) - lumResistor) * 0.2; //filtering the analog value
  //Serial.println(lumResistor);

  uint8_t lum = map(lumResistor, 300, 1023, 0, 255);
  lum=constrain(lum,30,255); //Sets a minimum to avoid having weird colors due to a too weak brightness

  //Read PPM from receiver
  int ppm = pulseIn(PPMPIN, HIGH, 50000); //PWM from GT2B receiver CH3 : about 1000 microseconds when off, 2000 when on.

  //Read wired momentary push button state
  pushBtn = !digitalRead(PUSHBTNPIN);

//  //Read PPM from CH3 channel (ON/OFF) of receiver for activating light
//  if (ppm > 1700) {
//    lightSt = true;
//    digitalWrite(BUILTINLEDPIN, HIGH);
//  }
//  else if (ppm < 1300) {
//    lightSt = false;
//    digitalWrite(BUILTINLEDPIN, LOW);
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
  //Serial.print("ShortPush ");
  //Serial.println (shortPush);
  //Serial.print("LongPush ");
  //Serial.println (longPush);

  //Read data and check communication status with VESC
  if (UART.getVescValues()) { //Succeed to communicate with VESC
    commVescSt = true;
    timerCommVesc = millis();
    resetTry = false;
    autoResetTry = false;
  }
  else if (millis() - timerCommVesc > COMM_VESC_TIMEOUT) { //Failed to communicate with VESC (after a delay)
    commVescSt = false;
  }

  if (commVescSt) { //VESC communication is alive
    digitalWrite(BUILTINLEDPIN, HIGH);
    //Process theses instructions in case of communication start or recovery

    if (UART.data.fault != 0 ){ // Fault occured: animation pattern
      maintainFaultDelay = millis();
    }
    if (millis() > FAULT_DISPLAY_TIME && millis() - maintainFaultDelay < FAULT_DISPLAY_TIME){

      clearRing();

      if (millis()-timerToogleFault > 400) {
        for (uint8_t z=0;z<UART.data.fault;z++){
        ring.setPixelColor(z+ringStartBit,255,0,0);
        }
        if (millis()-timerToogleFault > 1400) {
          timerToogleFault=millis();
        }
      }
      else if (millis()-timerToogleFault > 200) {
        for (uint8_t z=0;z<(NUM_LEDS_RING/2);z++){
        ring.setPixelColor(z+ringStartBit,255,0,0);
        }
      }
      else {
        for (int z=0;z<(NUM_LEDS_RING/2);z++){
        ring.setPixelColor(NUM_LEDS_RING+ringStartBit-z-1,255,0,0);
        }
      }
    }
    else { //VESC comm. alive and no fault
      float instantSpeed;
      uint8_t speedRatio,currentModeQuarter;
      bool brakeSt = false;

      int WhDischarged = int(UART.data.watt_hours - UART.data.watt_hours_charged);
#ifdef EN_COULOMB_COUNTING
      float cellVoltage = UART.data.inpVoltage / CELLS_NUMBER;


      if (!battCharged && UART.data.avgInputCurrent >=0.0 && cellVoltage >= BATT_FULL_THRESHOLD) { //Detect if the battery has been charged (and not in regen)
        resetCoulombCounter();
        setBattState(true);
      }
      else if (UART.data.inpVoltage<=(VOLTAGE_CUTOFF_END + CELLS_NUMBER * 0.05)) { //if voltage reaches cutoff end (plus hysteresis), store current coulomb counter as effective battery capacity
        setNewBattCap();
      }
      else if (battCharged && cellVoltage < BATT_FULL_THRESHOLD && UART.data.avgInputCurrent <= 1.0) { //State battery as uncharged if voltage below threshold (and no current drawn) for 10sec
        if(millis() - battUnchargedDelay > 10000){
          setBattState(false);
        }
      }
      else {
        battUnchargedDelay = millis();
      }
#endif
      if (initAnim) {
        for (uint8_t z=0; z<NUM_LEDS_RING;z++) {
          ring.setPixelColor(z+ringStartBit,0);
          ring.show();
          delay(100);
        }
        initAnim=false;
        page = 0;
        timerLongPush = millis();
        tachoMemo = UART.data.tachometerAbs;
        capacityMemo = WhDischarged;
      }

      //Measurement of battery voltage when idle
      if (UART.data.avgInputCurrent < 1.5) {
        if (millis()-idleVoltageDelay > 3000) {
          voltageIdle = UART.data.inpVoltage;
          idleVoltageDelay=millis();
        }
      }
      else {
        idleVoltageDelay=millis();
      }

      //Capacity based on overall idle voltage measurement
      uint8_t voltageCap = CapCheckPerc(voltageIdle, CELLS_NUMBER);
      //Coulomb counting (capacity based on mAh drawn)
      uint8_t ccCap = uint8_t(100 - (100 * getCoulombCounter() / INITIAL_BATT_CAPACITY));
      uint8_t usedCap = voltageCap; //select which calculation used for displaying

      //update EEPROM total distance and coulomb counter
#ifdef EN_COULOMB_COUNTING
      if (!UART.data.fault && (millis() - EEPROMdelay) > 10000) { //Authorize writing EEPROM if there is more than 10s since last writing and no fault (shutting down the board triggers an UNDER_VOLTAGE fault).
        if(abs(WhDischarged - capacityMemo)>COULOMB_INTERVAL_UPDATE) { //Write only if a fair amount of mAh has been discharged
          updateCoulombCounter(int(WhDischarged - capacityMemo));
          capacityMemo = WhDischarged;
        }
        uint16_t distanceMemo = (UART.data.tachometerAbs - tachoMemo) * ratio; //distance in meter
        if (distanceMemo > DISTANCE_INTERVAL_UPDATE) {
          addDistance(uint16_t(distanceMemo/100)); //store as 1 unit per 100m
          tachoMemo = UART.data.tachometerAbs;
        }
        EEPROMdelay = millis();
      }
#endif
      //Cutoff start display management TODO add delay to avoid voltage sag triggering animation
      if (memoCutOffReached || UART.data.inpVoltage <= VOLTAGE_CUTOFF_START) { //Battery low
        //memoCutOffReached = true; //Maintain the Cutoff state
        if (shortPush) {
            lightSt = !lightSt;
          }
        if (millis()-timerToogleLowBatt > 200) {
          toogle=!toogle;
          if (toogle) {
            for (uint8_t z=0;z<NUM_LEDS_RING;z++){
              ring.setPixelColor(z+ringStartBit,255,0,0);
            }
          }
          else {
            clearRing();
          }
          timerToogleLowBatt=millis();
        }
      }
      else { //Not battery low

        uint8_t nbPixel,tempPrct;

        if (longPush) { //Change the "page" (aka the pattern displayed on the ring)
          if (page == 1 && blinkLeds){
            modeMenuTimeout = millis();
            currentMode = modeSelect;
            blinkLeds = false;
          }
          else {
            page++;
            if (page > 1) {
            page=0;
            }
          }
          longPush = false;
        }

        switch (page){

          case 0 : {//default menu : battery gauge
            if (shortPush) {
              lightSt = !lightSt;
            }
            //Select the last pixel showed and blinking on the ring/strip/stick
            uint8_t lastPixel = selectLastPixel(usedCap);

            for(uint8_t i=NUM_LEDS_RING;i>lastPixel;i--) {
              uint8_t r=255/NUM_LEDS_RING*i;
              uint8_t g=255-r;
              uint8_t b=0;
              ring.setPixelColor(i-1+ringStartBit,r,g,b);
            }
            //Switch pixels off after the last selected
            if (lastPixel > 0) {
              for(uint8_t i=lastPixel;i>0;i--) {
                ring.setPixelColor(i-1+ringStartBit,0);
              }
            }
            break;
          }

          case 1 : { //modes menu
            //currentMode = UART.data.currentMode
            if (currentMode == 4){
              currentModeQuarter = 0;
            }
            else {
              currentModeQuarter = currentMode;
            }

            for(uint8_t i=0;i<3;i++) {
              ring.setPixelColor(i+currentModeQuarter*3+ringStartBit,modesColor[currentMode]);
            }

            if (shortPush) {
              modeMenuTimeout = millis();
              if (!blinkLeds) {
                blinkLeds = true;
                timerToogleMode = millis();
              }
              else {
                modeSelect++;
                if (modeSelect>3) {
                  modeSelect = 0;
                }
              }
            }

            if (blinkLeds){
              if (toogle) {
                for(uint8_t i=0;i<3;i++) {
                  ring.setPixelColor(i+modeSelect*3+ringStartBit,0);
                }
              }
              else {
                for(uint8_t i=0;i<3;i++) {
                  ring.setPixelColor(i+modeSelect*3+ringStartBit,modesColor[modeSelect]);
                }
              }
              if ((millis() - timerToogleMode) > 200){
                timerToogleMode = millis();
                toogle = !toogle;
              }
            }

            if ((millis() - modeMenuTimeout) > 5000){
              page = 0;
              blinkLeds = 0;
            }
            break;
          }

          // case 1 : //Speed indicator : vertical symetry gauge from bottom to top (0 km/h to MAX_SPEED - set in config.h)
          //   if (shortPush) {
          //     lightSt = !lightSt;
          //   }
          //
          //   nbPixel = (uint8_t)(NUM_LEDS_RING / 2 * speedRatio / 100.0);
          //   redValFade = (uint8_t)(speedRatio*255.0/100.0);
          //   greenValFade = 0;
          //   blueValFade = 255;
          //   ring.clear();
          //   for(uint8_t i=0; i<=nbPixel; i++) {
          //     ring.setPixelColor(NUM_LEDS_RING / 2 - 1 - i + ringStartBit, redValFade, greenValFade, blueValFade);
          //     ring.setPixelColor(NUM_LEDS_RING / 2 + i + ringStartBit, redValFade, greenValFade, blueValFade);
          //   }
          //
          //   break;

          // case 2 : //Temperature indicator (empty ring = 20°C, full ring = 100°C), counterclockwise from blue to red
          //   if (shortPush) {
          //     lightSt = !lightSt;
          //   }
          //   ring.clear();
          //   if (measuredValues.tempMotor > 0.0) { //fulfill the whole ring in light white if ok, orange if the 80°C limit is reached (VESC is regulating amps)
          //     if (measuredValues.tempMotor > 80.0) {
          //       for(uint8_t i = toogle ? 1 : 0 ; i<NUM_LEDS_RING; i+=2) {
          //         ring.setPixelColor(i+ringStartBit, 255, 170, 0);
          //         toogle=!toogle;
          //       }
          //     }
          //     else {
          //       for(uint8_t i = 0 ; i<NUM_LEDS_RING; i+=2) {
          //         ring.setPixelColor(i+ringStartBit, 130, 130, 130);
          //       }
          //     }
          //   }
          //   else { //No motor temperature sensors or wire break
          //     for(uint8_t i = 0 ; i<NUM_LEDS_RING; i+=2) {
          //         ring.setPixelColor(i+ringStartBit, 255, 0, 70);
          //     }
          //   }
          //
          //   break;

          default:
            break;

        }
        // Rear/Headlight management
        if(UART.getMasterVescPPM(MASTER_VESC_ID)){ //Request PPM values to Master VESC over CANbus
          if (UART.data.throttlePPM < -PPM_DEADBAND) {
            brakeSt=true;
          }
          else if (UART.data.throttlePPM >= 0.0){
            brakeSt=false;
          }
        }
        else {
          if (UART.data.avgMotorCurrent < -BRAKE_CURRENT_DEADBAND) {
            brakeSt=true;
          }
          else if (UART.data.avgMotorCurrent > -0.01){
            brakeSt=false;
          }
        }

#ifdef LIGHTS_ALWAYS_ON
        lightSt = true;
#endif
        rearLight(brakeSt,lightSt);
        headLight(lightSt);
      }
    }
  }
  else { //VESC communication lost
    initAnim=true; //This will trigger the animation transition once the VESC communication will recover
    digitalWrite(BUILTINLEDPIN, LOW);

    if (shortPush){
      lightSt = !lightSt;
    }

     if(!autoResetTry && (millis() - timerCommVesc) > 5000) {
      autoResetTry = true;
      resetComm();
     }

     //Pattern : chase
     clearRing();

    if (chenilCount==(NUM_LEDS_RING-2)) {
      ring.setPixelColor(10+ringStartBit, 75, 0, 130);
      ring.setPixelColor(11+ringStartBit, 75, 0, 130);
      ring.setPixelColor(0+ringStartBit, 75, 0, 130);
    }
    else if (chenilCount==(NUM_LEDS_RING-1)) {
      ring.setPixelColor(11+ringStartBit, 75, 0, 130);
      ring.setPixelColor(0+ringStartBit, 75, 0, 130);
      ring.setPixelColor(1+ringStartBit, 75, 0, 130);
    }
    else {
      for (uint8_t k=0; k<3; k++) {
          ring.setPixelColor(chenilCount+k+ringStartBit,75, 0, 130);
        }
    }
#ifdef LIGHTS_ALWAYS_ON
    lightSt = true;
#endif
    rearLight(false,lightSt);
    headLight(lightSt);

    if (millis() - timerChenil >= 250) {
      chenilCount++;
      timerChenil = millis();
      if (chenilCount==NUM_LEDS_RING){
        chenilCount=0;
      }
    }

    if (longPush && !resetTry){
      resetComm();
      resetTry = true;
    }
    longPush = false;
  }

  //rightTurnLight();
  //adaptBrightness(lum); //This void because it's not recommended to use set.Brightness() in the loop, only once in setup.
  ring.show();

  delay(50);
}

//Selection of the last pixel according to the capacity
uint8_t selectLastPixel (uint8_t value) {

  //At least 1 led displayed even if capacity = 0
    for(uint8_t z=1; z<=NUM_LEDS_RING;z++) {
      if (value <= ringArray[z]) {
        return (NUM_LEDS_RING - z);
      }
      else if (z >= NUM_LEDS_RING) {
        return 0;
      }
    }
}

void adaptBrightness(uint8_t value) {

  for (uint8_t z=0;z<NUM_LEDS_RING;z++) {
    uint8_t redVal, greenVal, blueVal;
    uint32_t currentColor;
    currentColor = ring.getPixelColor(z+ringStartBit);

    redVal = red(currentColor) * float(value/255.0);
    greenVal = green(currentColor) * float(value/255.0);
    blueVal = blue(currentColor) * float(value/255.0);

    ring.setPixelColor(z+ringStartBit, greenVal, redVal, blueVal);
  }
}

void rearLight(bool brake, bool light) { //Rearlight management

  if (brake) {
    for (uint8_t z=0; z<NUM_LEDS_REARLIGHT; z++){
        ring.setPixelColor(z+stickStartBit,255, 0, 0); //Set red full brightness
    }
  }
  else if (light){
    // for (uint8_t z=0; z<NUM_LEDS_REARLIGHT; z+=2){
    //     ring.setPixelColor(z+stickStartBit,150,0,0);
    // }
    //For Colab
    for (uint8_t z=0; z<NUM_LEDS_REARLIGHT; z++){
        ring.setPixelColor(z+stickStartBit, 0);
    }
    for (uint8_t z=0; z<8; z+=2){
        ring.setPixelColor(z+stickStartBit,100,0,0);
    }
    for (uint8_t z=9; z<NUM_LEDS_REARLIGHT; z+=2){
        ring.setPixelColor(z+stickStartBit,100,0,0);
    }
  }
  else {
    for (uint8_t z=0; z<NUM_LEDS_REARLIGHT; z++){
        ring.setPixelColor(z+stickStartBit,0);
    }
  }
}

void headLight(bool status) { //Headlight management
#ifdef IS_HEADLIGHT
  if (status) {//power headlight ON
    for (uint8_t z=0; z<NUM_LEDS_HEADLIGHT; z++){
        ring.setPixelColor(z+headLightStartBit,255, 255, 255); //Set white full brightness
    }
  }
  else { //power headlight OFF
    for (uint8_t z=0; z<NUM_LEDS_HEADLIGHT; z++){
        ring.setPixelColor(z+headLightStartBit, 0);
    }
  }
#endif
}

void clearRing(){
  for (uint8_t i=0;i<NUM_LEDS_RING;i++){
    ring.setPixelColor(i+ringStartBit,0);
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

void resetComm() {
  Serial.end();

  for (uint8_t cnt; cnt<3; cnt++) {
    for (uint8_t z=0; z < NUM_LEDS_RING; z++) {
      ring.setPixelColor(z+ringStartBit, 75, 0, 130);
    }
    ring.show();
    delay(200);
    clearRing();
    ring.show();
    delay(200);
  }

  Serial.begin(BAUDRATE);

  for (uint8_t z=0; z < NUM_LEDS_RING; z++) {
    ring.setPixelColor(z+ringStartBit, 75, 0, 130);
  }
  ring.show();
  delay(500);
}

void updateCoulombCounter(int value){ //store in EEPROM the addtionnal amount of mAh discharged
  uint16_t memo = getEEPROMvalues(ADDR_TOT_WH);
  setEEPROMvalues(ADDR_TOT_WH,uint16_t(int(memo)+value));
}

uint16_t getCoulombCounter() { //read EEPROM value of coulomb counting
  uint16_t counter = 0;
  if (counter == 0){
    counter = getEEPROMvalues(ADDR_TOT_WH);

    return counter;
  }
}

void resetCoulombCounter(void){
  setEEPROMvalues(ADDR_TOT_WH,0);
}

void setNewBattCap(void){
  setEEPROMvalues(getCoulombCounter(),ADDR_REAL_CAPACITY);
}

bool getBattState(void){
  bool state = false;
  if (EEPROM.read(ADDR_BATT_STATE)==1){
    state = true;
  }
  return state;
}

void setBattState(bool isCharged){
  if (isCharged) {
    EEPROM.write(ADDR_BATT_STATE,1);
    battCharged = true;
  }
  else {
    EEPROM.write(ADDR_BATT_STATE,0);
    battCharged = false;
  }
}

void addDistance(uint16_t value){
  uint16_t memo = getEEPROMvalues(ADDR_TOT_DISTANCE);
  setEEPROMvalues(ADDR_TOT_DISTANCE,memo+value);
}

uint16_t getEEPROMvalues(uint8_t address) { //read EEPROM at the specified address
  uint16_t value=0;

  value += EEPROM.read(address+1) << 8;
  value += EEPROM.read(address);

  return value;
}

void setEEPROMvalues(uint8_t address,uint16_t data) { //write EEPROM with uint16_t data at the specified address
  EEPROM.write(address+1,data >> 8);
  EEPROM.write(address,data & 0xFF);
}

void rightTurnLight(){
  if ((millis() - turnLightTimer) >= 600){ // french regulations : 90 pulses per minute
    toogleTurnLight != toogleTurnLight;
    turnLightTimer = millis();
  }

  if (toogleTurnLight){
    for (uint8_t z=0; z < 8; z++) {
      ring.setPixelColor(z+stickStartBit, 255, 50, 0);
    }
    for (uint8_t z=0; z < 3; z++) {
      ring.setPixelColor(z+headLightStartBit, 255, 50, 0);
    }
  }
  else{
    for (uint8_t z=0; z < 3; z++) {
      ring.setPixelColor(z+headLightStartBit, 0);
    }
  }
}
