// config.h


#ifndef _CONFIG_h
#define _CONFIG_h

#define BAUDRATE 115200 //Baudrate for UART communication with VESC

//Configuration depending on user setup and preferences
#define CELLS_NUMBER 12 //Cells number in series of battery pack (ex : 10 for 10S3P Li-Ion)
#define MAX_SPEED 42 //Max speed corresponding to a full red ring in speed indicator mode
#define GEAR_RATIO 0.200 //Gear ratio. To calculate : motor pulley tooth number divided by wheel pulley tooth number. E.g : 15/36=0.417
#define WHEEL_DIAM 228 // Wheel
#define BATT_TYPE 0 //Battery chemistry (0 = lipo; 1 = li-ion)
#define LIGHTS_ALWAYS_ON //Forces rearlight and headlight to be always ON

//VESC Config
#define INITIAL_BATT_CAPACITY 427 //Usable battery capacity in Wh (= S x Ah x (3,6 for Liion, 3,7 for lipo) x 0,8 (usable is often 80% of the rated capacity))
#define VOLTAGE_CUTOFF_START 43.2 //Voltage Cutoff start as set in VESC Master
#define VOLTAGE_CUTOFF_END 40.8 //Voltage Cutoff end as set in VESC Master
#define MASTER_VESC_ID 0 //In case that SmartRing is connected to Slave VESC, some infos need to be asked to Master through CANbus

#define FRONTLIGHT_PWR 100 //Frontlight power when board is stopped
#define FRONTLIGHT_MAXPWR 180 //Frontlight max power
#define AUTOLIGHT_ON 100 //Ambient luminosity for activating frontlight
#define AUTOLIGHT_OFF 200 //Ambient luminosity for desactivating frontlight


//Accessories setting
#define NUM_LEDS_REARLIGHT 16 //number of rear light stick
#define NUM_LEDS_RING 12 //number of LEDS of gauge (ring)
#define NUM_LEDS_HEADLIGHT 12 //number of leds in case of frontlight
#define NUM_LEDS_TOTAL 40 //total number of WS2812 leds (= sum of values above)

#define REARLIGHT_FIRST //if rear light stick is wired first in the data chain (PCB -> DIN Stick DOUT-> DIN ring), uncomment this line
#define IS_HEADLIGHT //if WS2812 leds are wired after the ring (if ring is wired after rearlight), use them as frontlight. Comment this line if not present

//User preferences
#define BATT_FULL_THRESHOLD 4.10 //Threshold above which the battery is considered as full (for SoC calculation)
#define DISTANCE_INTERVAL_UPDATE 200 //value in meter for trigering an update of EEPROM total distance value
#define COULOMB_INTERVAL_UPDATE 200 //value in mAh for trigering an update of EEPROM total distance value
#define BRAKE_CURRENT_DEADBAND 1.0 //brake current above which brake light are switched on
#define COMM_VESC_TIMEOUT 2000 //delay in ms for considering VESC communication as lost
#define FAULT_DISPLAY_TIME 5000 //Delay of display when a fault occurs
//#define COULOMB_COUNTING //Enable the coulomb counting
#define PPM_DEADBAND 10.0 //PPM deadband as programmed in VESC Tool (avoid having rearlight shuttering)

#endif
