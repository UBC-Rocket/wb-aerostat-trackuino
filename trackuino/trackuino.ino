/* trackuino copyright (C) 2010  EA5HAV Javi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// Mpide 22 fails to compile Arduino code because it stupidly defines ARDUINO 
// as an empty macro (hence the +0 hack). UNO32 builds are fine. Just use the
// real Arduino IDE for Arduino builds. Optionally complain to the Mpide
// authors to fix the broken macro.
#if (ARDUINO + 0) == 0
#error "Oops! We need the real Arduino IDE (version 22 or 23) for Arduino builds."
#error "See trackuino.pde for details on this"

// Refuse to compile on arduino version 21 or lower. 22 includes an 
// optimization of the USART code that is critical for real-time operation
// of the AVR code.
#elif (ARDUINO + 0) < 22
#error "Oops! We need Arduino 22 or 23"
#error "See trackuino.pde for details on this"

#endif


// Trackuino custom libs
#include "config.h"
#include "afsk_avr.h"
#include "afsk_pic32.h"
#include "aprs.h"
#include "buzzer.h"
#include "gps.h"
#include "pin.h"
#include "power.h"
#include "sensors_avr.h"
#include "sensors_pic32.h"

//Aerostat-specific
#include "aerostat_utils.h"
#include "windSensor.h"
#include "barometer.h"
#include "adaUlGps.h"

// Arduino/AVR libs
#if (ARDUINO + 1) >= 100
#  include <Arduino.h>
#else
#  include <WProgram.h>
#endif


// Module constants
static const uint32_t VALID_POS_TIMEOUT = 2000;  // ms

// Module variables
static unsigned long int measure_timer = 0;
static unsigned long int aprs_timer = 0; // Defined around line 107

// Variables for storing multiple measurements per measurement period.
int velocityValues[MEASUREMENTS_PER_PERIOD];
int altitudeValues[MEASUREMENTS_PER_PERIOD];
int measurementIndex= 0; //which index of altitude and velocity arrays to fill


void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pin_write(LED_PIN, LOW);

  Serial.begin(115200);
#ifdef DEBUG_RESET
  Serial.println("RESET");
#endif

  //buzzer_setup();
  afsk_setup();
  setupBarometer();
  setupAdaUlGps();


#ifdef DEBUG_SENS
  Serial.print("Ti=");
  Serial.print(sensors_int_lm60());
  Serial.print(", Te=");
  Serial.print(sensors_ext_lm60());
  Serial.print(", Vin=");
  Serial.println(sensors_vin());
#endif
  
  // Do not start until we get a valid time reference
  // for slotted transmissions.
  
  aprs_timer = millis();
  measure_timer = millis(); 
  
  if (APRS_SLOT >= 0) {
//    do {
//      while (! Serial.available())
//        power_save();
//    } while (! gps_decode(Serial.read()));
//    
//    aprs_timer = millis() + 1000 *
//      (APRS_PERIOD - (gps_seconds + APRS_PERIOD - APRS_SLOT) % APRS_PERIOD);
  }
  
  // TODO: beep while we get a fix, maybe indicating the number of
  // visible satellites by a series of short beeps?*/
  Serial.println("Setup successful");
}


void loop()
{
   char gpsString[50];
   int altMeasurement;
   adaUlRecievePosition(&measure_timer, gpsString, 49, &altMeasurement);

  // Time for another measurement
  if ((millis() - measure_timer) >= (APRS_PERIOD/MEASUREMENTS_PER_PERIOD*1000))
  {
    //add another wind speed measurement to velocityValues, and altitude measurement to velocityValues
    velocityValues[measurementIndex] = (int)measureRevpWind();
    altitudeValues[measurementIndex] = altMeasurement;
   
    measurementIndex++; 

    measure_timer = millis();
    Serial.println("Measured");
  }

  
  // Time for another APRS frame
  if ((millis() - aprs_timer) >= APRS_PERIOD*1000) {
    Serial.println(millis() - aprs_timer);
    aprs_send(gpsString, altitudeValues, velocityValues);
    measurementIndex = 0;


    aprs_timer = millis();
    while (afsk_flush()) {
      power_save();
    }

#ifdef DEBUG_MODEM
    // Show modem ISR stats from the previous transmission
    afsk_debug();
#endif
  }
}
