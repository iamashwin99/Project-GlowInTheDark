#ifndef CONFIG_H_
#define CONFIG_H_


// --- Includes ---
#include <Arduino.h>

// --- Constants ---
const uint16_t NUMPIXELS = 72;  // Number of LEDs in strip

const char* SSID = "Shiny";            // WiFi SSID
const char* PASSWORD = "Dull";  // WiFi password

const uint8_t LED_PIN = 0;

const uint8_t DOTSTAR_DATA_PIN = 4;
const uint8_t DOTSTAR_CLOCK_PIN = 5;


#endif /* CONFIG_H_ */
