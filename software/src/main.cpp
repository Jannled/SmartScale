#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>

#include "main.hpp"
#include "menuConfig.hpp"
#include "smartOta.hpp"
#include "index.html.h"

#include "math.h"

HX711 hx711;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float currentWeight = NAN;

unsigned long time01 = millis();

bool ledState = false;

/**
 * @brief Handle 404 for the webserver
 * 
 * @param request 
 */
void notFound(AsyncWebServerRequest *request) {
	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("[Web] %8lu: %d %s %s \n", millis(), 404, request->methodToString(), request->url().c_str());
	#endif
    request->send(404, "text/plain", "Not found");
}

/**
 * @brief Arduino Setup, called once at boot
 * 
 */
void setup()
{
	#ifdef ENABLE_OTA
	setupOTA();
	#endif

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	EEPROM.begin(EEPROM_SIZE);

	#ifdef ENABLE_ALL_SERIAL
	Serial.begin(115200);
	delay(500);
	Serial.println();
	Serial.println(" --- Smart Kitchen Scale --- ");
	#endif

	/* --- Init EEPROM and read values --- */
	long hx711_offset = 0;
	float hx711_divider = 1;
	EEPROM.get(ADDR_OFFSET, hx711_offset);
	EEPROM.get(ADDR_DIVIDER, hx711_divider);

	if(isnan(hx711_divider) || (hx711_divider == 0.0)) 
	{
		hx711_offset = 0;
		hx711_divider = 1.0f;
	}

	/* --- Init HX711 --- */
	hx711.begin(HX711_DOUT, HX711_SCK);
	hx711.power_up(); // Make sure the chip is turned on
	hx711.set_offset(hx711_offset);
	hx711.set_scale(hx711_divider);

	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("Initialized HX711 with offset %ld and divider %.6f\n", hx711_offset, hx711_divider);
	initMenu();
	printMenuInfo();
	#endif

	// Wait for the modem to become ready
	delay(1500);

	/* --- Init WiFi and Webserver --- */
	//WiFi.setHostname("SmartKitchenScale");
	WiFi.begin(); // Empty SSID and password should mean this thing will reconnect to the last known network
	server.onNotFound(notFound);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		#ifdef ENABLE_ALL_SERIAL
		Serial.printf("[Web] %8lu: %d %s %s \n", millis(), 200, request->methodToString(), request->url().c_str());
		#endif
		// WARNING: THE STRING NEEDS TO BE \0 TERMIANTED!!!
        request->send(200, "text/html", (const char*) index_html);
    });
	server.begin();

	/* --- Init Websocket --- */
	ws.onEvent(onEvent);
	server.addHandler(&ws);
}

/**
 * @brief Arduino Loop, called as often as possible
 * 
 */
void loop()
{
	#ifdef ENABLE_OTA
	loopOTA();
	#endif

	digitalWrite(LED_BUILTIN, ledState = !ledState);
	static wl_status_t lastWiFiState = WiFi.status();

	wl_status_t wifiState = WiFi.status();
	if(lastWiFiState != wifiState)
		printWiFiStatus(true);
	lastWiFiState = wifiState;

	ws.cleanupClients();

	unsigned long currTime = millis();
	if(abs((long) (currTime - time01)) > 500)
	{
		time01 = currTime;
		currentWeight = hx711.get_units(5);
		notifyClients();
	}

	#ifdef ENABLE_ALL_SERIAL
	updateMenu();
	#endif
}

/**
 * @brief Send the current weight to all Websocket subscribers
 * 
 */
void notifyClients()
{
	ws.textAll(String((int) currentWeight));
}

/**
 * @brief Websocket receive
 * 
 * @param arg 
 * @param data 
 * @param len 
 */
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
	AwsFrameInfo *info = (AwsFrameInfo *)arg;
	if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
	{
		data[len] = 0; // Propably to \0 terminate the String
		if (strcmp((char *)data, "tare") == 0)
		{
			tare();
		}
	}
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
			 void *arg, uint8_t *data, size_t len)
{
	switch (type)
	{
		case WS_EVT_CONNECT:
			#ifdef ENABLE_ALL_SERIAL
			Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
			#endif
			break;
		case WS_EVT_DISCONNECT:
			#ifdef ENABLE_ALL_SERIAL
			Serial.printf("[WS] Client #%u disconnected\n", client->id());
			#endif
			break;
		case WS_EVT_DATA:
			handleWebSocketMessage(arg, data, len);
			break;
		case WS_EVT_PONG:
		case WS_EVT_ERROR:
			break;
	}
}

String processor(const String &var)
{
	Serial.println(var);
	if (var == "STATE")
	{
		return String(currentWeight);
	}

	return ">>NONE<<";
}

void tare() {
	hx711.tare();
	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("Tare, new offset is %.ld\n", hx711.get_offset());
	#endif
}