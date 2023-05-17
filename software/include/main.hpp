#ifndef SMARTSCALE_MAIN_H
#define SMARTSCALE_MAIN_H

// Includes
#include <HX711.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Enable or disable some features
#define ENABLE_ALL_SERIAL
//#define ENABLE_OTA

// Pin definitions
#define HX711_DOUT 25
#define HX711_SCK 26

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// EEPROM Settings
#define EEPROM_SIZE 64
#define ADDR_OFFSET 0
#define ADDR_DIVIDER 8

// Stuff
extern HX711 hx711;
extern AsyncWebServer server;
extern AsyncWebSocket ws;

extern float currentWeight;

extern bool ledState;

void notifyClients();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
String processor(const String &var);

void tare();

float calibrateGain(unsigned int knownReference);
long calibrateOffset();
void saveCalibration();

void callback();

bool createHotspot();

#endif // SMARTSCALE_MAIN_H