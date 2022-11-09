//
// Created by david on 11/4/2022.
//

#ifndef NANOLUX_WEBSERVER_H
#define NANOLUX_WEBSERVER_H

#define DEBUG_WIFI_WEBSERVER_PORT   Serial

// Debug Level from 0 to 4
#define _WIFI_LOGLEVEL_             4
#define _WIFININA_LOGLEVEL_         3

#define USE_WIFI_NINA         false

// To use the default WiFi library here
#define USE_WIFI_CUSTOM       false

#warning Using ESP WiFi with WiFi Library
#define SHIELD_TYPE           "ESP WiFi using WiFi Library"

#warning ESP32 board selected
#define BOARD_TYPE  "ESP32"


#include <WiFiWebServer.h>

char ssid[] = "AUDIOLUX";        // your network SSID (name)
char pass[] = "12345678";        // your network password

#endif //NANOLUX_WEBSERVER_H
