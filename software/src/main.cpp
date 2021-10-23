#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>

#include "main.hpp"
#include "menuConfig.hpp"
#include "index.html.h"

HX711 hx711;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool ledState = false;

void notFound(AsyncWebServerRequest *request) {
	Serial.printf("[Web] %8lu: %d %s %s \n", millis(), 404, request->methodToString(), request->url().c_str());
    request->send(404, "text/plain", "Not found");
}

void setup() 
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	Serial.begin(115200);
	delay(100);
	Serial.println();
	Serial.println(" --- Smart Kitchen Scale ---");

	/* --- Init EEPROM and read values --- */
	uint16_t hx711_offset = 0;
	uint16_t hx711_divider = 1;
	EEPROM.begin(512);
	//EEPROM.get(ADDR_OFFSET, hx711_offset);
	//EEPROM.get(ADDR_DIVIDER, hx711_divider);

	/* --- Init HX711 --- */
	hx711.begin(HX711_DOUT, HX711_SCK);
	hx711.power_up(); // Make sure the chip is turned on
	hx711.set_offset(hx711_offset);
	hx711.set_scale(hx711_divider);

	Serial.printf("Initialized HX711 with offset %d and divider %d\n", hx711_offset, hx711_divider);
	Serial.println("To open the configuration menu send an 'm' over the serial console.");

	/* --- Init WiFi and Webserver --- */
	//WiFi.setHostname("SmartKitchenScale");
	WiFi.begin(); // Empty SSID and password should mean this thing will reconnect to the last known network
	server.onNotFound(notFound);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		Serial.printf("[Web] %8lu: %d %s %s \n", millis(), 200, request->methodToString(), request->url().c_str());
        request->send(200, "text/html", (const char*) index_html);
    });
	server.begin();

	/* --- Init Websocket --- */
	
}

void loop()
{
	digitalWrite(LED_BUILTIN, ledState = !ledState);
	static wl_status_t lastWiFiState = WiFi.status();

	wl_status_t wifiState = WiFi.status();
	if(lastWiFiState != wifiState)
		printWiFiStatus(true);
	lastWiFiState = wifiState;

	updateMenu();
}

void notifyClients()
{
	ws.textAll("Test");
}