// config.h


#ifndef _CONFIG_h
#define _CONFIG_h

//Configuration depending on user setup and preferences
#define CELLS_NUMBER 8 //Cells number in series of battery pack (ex : 10 for 10S3P Li-Ion)
#define LOW_BAT_LEVEL 10 //battery level for triggering low battery animation (whole ring/strip/stick blinking in red)
#define MAX_SPEED 40 //Max speed corresponding to a full red ring in speed indicator mode
#define GEAR_RATIO 0.278 //Gear ratio. To calculate : motor pulley tooth number divided by wheel pulley tooth number. E.g : 15/36=0.417
#define WHEEL_DIAM 203 // Wheel
#define BATT_TYPE 0 //Battery chemistry (0 = lipo; 1 = li-ion)

#define FRONTLIGHT_PWR 100 //Frontlight power when board is stopped
#define FRONTLIGHT_MAXPWR 180 //Frontlight max power
#define AUTOLIGHT_ON 100 //Ambient luminosity for activating frontlight
#define AUTOLIGHT_OFF 200 //Ambient luminosity for desactivating frontlight

#define NUM_LEDS_RING 12 //number of LEDS of gauge (ring)
#define NUM_LEDS_REARLIGHT 8 //number of rear light stick
#define NUM_LEDS_TOTAL 20 //total number of WS2812 leds
#define REARLIGHT_FIRST //if rear light stick is wired first in the data chain (PCB -> DIN Stick DOUT-> DIN ring), uncomment this line

#endif
