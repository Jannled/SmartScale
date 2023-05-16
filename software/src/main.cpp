#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>

#include "main.hpp"
#include "menuConfig.hpp"
#include "smartOta.hpp"
#include "index.html.h"
#include "secrets.h"

#include "math.h"

HX711 hx711;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

float currentWeight = NAN;

/** Timer for weight updates */
unsigned long time01 = millis();

/** Timer for shutting down SmartScale */
unsigned long time02 = millis();

/** Time of last connection. Used for creating a station if unable to connect to WiFi */
unsigned long time03 = millis();

bool ledState = false;

#define TOUCH_PIN T9
touch_value_t touchThreshold = 40;

char ssidAP[32] = "SmartScale";

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

	// Endpoint 404
	server.onNotFound(notFound);

	// Endpoint / aka index.html
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		#ifdef ENABLE_ALL_SERIAL
		Serial.printf("[Web] %8lu: %d %s %s \n", millis(), 200, request->methodToString(), request->url().c_str());
		#endif
		// WARNING: THE STRING NEEDS TO BE \0 TERMINATED!!!
        request->send(200, "text/html", (const char*) index_html);
    });

	// Endpoint /connect
	server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
		if(!request->hasParam("ssid", true) || !request->hasParam("pswd", true))
			return request->send(400, "text/html", (const char*) "Missing parameter");

		request->send(200, "text/html", (const char*) "OK");

		char ssid[32] = {0};
		strncpy(ssid, request->getParam("ssid")->value().c_str(), 31);

		char passwd[64] = {0};
		strncpy(passwd, request->getParam("pswd")->value().c_str(), 63);

		// Don't assume that ESP32s Web library handles string length of 32/64 and missing \0 characters correctly
		#ifdef ENABLE_ALL_SERIAL
		Serial.printf("[Web] Trying to connect to \"%s\"...", ssid);
		#endif
		WiFi.begin(ssid, passwd);
	});

	// Endpoint /calibrate
	server.on("/calibrate", HTTP_POST, [](AsyncWebServerRequest *request){
		if(!request->hasParam("op", true))
			return request->send(400, "text/html", (const char*) "Missing parameter");

		char opCode[8] = {0};
		strncpy(opCode, request->getParam("op")->value().c_str(), 7);
		
		if (strcmp(opCode, "gain") == 0)
		{
			strncpy(opCode, request->getParam("val")->value().c_str(), 7);
			int knownWeight = atoi(opCode);

			if(knownWeight <= 0)
				return request->send(400, "text/html", (const char*) "Weight must be > 0!");

			calibrateGain(knownWeight);
		}
		else if (strcmp(opCode, "offset") == 0)
			calibrateOffset();
		else if (strcmp(opCode, "save") == 0)
			saveCalibration();
	});

	// Start the server
	server.begin();

	/* --- Init Websocket --- */
	ws.onEvent(onEvent);
	server.addHandler(&ws);

	// Add last two digits of MAC address to AP-SSID 01:34:67:9A:CD:F0
	uint8_t mac[6];
	WiFi.macAddress(mac);
	snprintf(ssidAP, 32, "SmartScale-%02X%02X", mac[4], mac[5]);

	// Try to start the OTA listener. Method is blocking until a WiFi state is established
	startOTA();
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
	{
		#ifdef ENABLE_ALL_SERIAL
		printWiFiStatus(true);
		#endif
	}
	lastWiFiState = wifiState;

	ws.cleanupClients();

	// Get current timestamp since start
	unsigned long currTime = millis();

	// Write time of last connection
	if(wifiState == WL_CONNECTED)
		time03 = currTime;

	// Send weight updates in fixed time intervals
	if(abs((long) (currTime - time01)) > 500)
	{
		time01 = currTime;
		currentWeight = hx711.get_units(5);
		notifyClients();
	}

	// Shutdown SmartScale after x minutes
	if(abs((long) (currTime - time02)) > 1200000) // 20min
	{
		#ifdef ENABLE_ALL_SERIAL
		Serial.println("SmartScale was inactive for 20min, shutting down!");
		#endif
		//shutdown();
	}

	// If unconnected for too long, create AP
	if(abs((long) (currTime - time03)) > 40000) // 40sec
	{
		time03 = currTime;
		createHotspot();
		Serial.print("[WiFi] HotSpot IP: ");
		Serial.println(WiFi.softAPIP());
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
		data[len] = 0; // Probably to \0 terminate the String
		if (strcmp((char *)data, "tare") == 0)
			tare();
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

float calibrateGain(unsigned int knownReference)
{
	float measurement = hx711.get_units(10);
	float divider = measurement / (float) knownReference;
	hx711.set_scale(divider);
	return divider;
}

long calibrateOffset()
{
	const long offset = hx711.read_average(10);
	hx711.set_offset(offset);
	return offset;
}

void saveCalibration()
{
	const long offset = hx711.get_offset();
	const float divider = hx711.get_scale();

	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("Calibration done. Offset: %ld, Divider: %.2f.\n", offset, divider);
	#endif

	EEPROM.writeLong(ADDR_OFFSET, offset);
	EEPROM.writeFloat(ADDR_DIVIDER, divider);
	delay(200); // Necessary to save
	EEPROM.commit();

	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("Values written to EEPROM address %d-%d and %d-%d.\n", ADDR_OFFSET, ADDR_OFFSET+sizeof(offset), ADDR_DIVIDER, ADDR_DIVIDER+sizeof(offset));
	#endif

	long hx711_offset = 0;
	float hx711_divider = 0.0f;
	EEPROM.get(ADDR_OFFSET, hx711_offset);
	EEPROM.get(ADDR_DIVIDER, hx711_divider);

	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("%ld, %.2f\n", hx711_offset, hx711_divider);
	#endif
}

void tare() {
	hx711.tare();
	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("Tare, new offset is %.ld\n", hx711.get_offset());
	#endif
}

void shutdown()
{
	// Determine a good threshold value
	touchThreshold = touchRead(TOUCH_PIN) + 10;
	touchAttachInterrupt(TOUCH_PIN, callback, touchThreshold);

	//Configure Touchpad as wakeup source
	esp_sleep_enable_touchpad_wakeup();

	#ifdef ENABLE_ALL_SERIAL
	Serial.println("Entering deep sleep");
	#endif

	//Go to sleep now
	esp_deep_sleep_start();
}

void resetShutdownTimer()
{
	time02 = millis();
}

void callback()
{
	#ifdef ENABLE_ALL_SERIAL
	Serial.println("Touch detected!");
	#endif
}

bool createHotspot()
{
	static bool enabledHotspot = false;

	if(enabledHotspot)
		return false;

	#ifdef ENABLE_ALL_SERIAL
	Serial.printf("[WiFi] Creating HotSpot \"%s\" \n", ssidAP);
	delay(100);
	#endif

	enabledHotspot = WiFi.softAP(ssidAP, AP_PASSWD);
	WiFi.softAPgetStationNum();
	return enabledHotspot;
}
