#ifndef SMARTSCALE_MAIN_H
#define SMARTSCALE_MAIN_H

#include <HX711.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define HX711_DOUT 25
#define HX711_SCK 26

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#define ADDR_OFFSET 0
#define ADDR_DIVIDER 2

extern HX711 hx711;
extern AsyncWebServer server;
extern AsyncWebSocket ws;

extern bool ledState;

#endif // SMARTSCALE_MAIN_H