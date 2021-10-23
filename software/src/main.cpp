#include <Arduino.h>
#include <HX711.h>
#include <EEPROM.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define HX711_DOUT 25
#define HX711_SCK 26

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#define ADDR_OFFSET 0
#define ADDR_DIVIDER 2

HX711 hx711;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

uint8_t state = 0;

unsigned long t = 0;

bool ledState = false;

/**
 * Print the current WiFi status to the Serial Conosle
 * @param verbose Print additional information
 */
void printWiFiStatus(bool verbose = false);

size_t serialReadLine(char *buffer, size_t length);

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
        request->send(200, "text/plain", "Hello, world");
    });
	server.begin();


}

void loop()
{
	digitalWrite(LED_BUILTIN, ledState = !ledState);
	static wl_status_t lastWiFiState = WiFi.status();

	wl_status_t wifiState = WiFi.status();
	if(lastWiFiState != wifiState)
		printWiFiStatus(true);
	lastWiFiState = wifiState;

	switch(state)
	{
		/* Normal operation */
		case 1:
			if (hx711.is_ready())
			{
				float val = hx711.get_units(5);
				Serial.printf("Gewicht: %.2fg\n", val);
				delay(1500);
			}
			else
				Serial.println("Timeout, check MCU => HX711 wiring and pin designations");
		// Please note the fallthrough (missing break), this way the menu will work in both states
		
		/* Silent operation */
		case 0:
			switch(Serial.read())
			{
				case 'm':
					state = 2;
					break;

				case 't':
					hx711.tare();
					Serial.println("Tare");
					delay(500);
					break;
					
				default:
					break;
			}
			break;
		
		/* SmartScale Menu */
		case 2:
			Serial.println("Choose one of the following options by entering their number:");
			Serial.println("1. Output current weight readings");
			Serial.println("2. Calibrate Scale");
			Serial.println("3. Change WiFi SSID and Password");
			Serial.println("4. Print WiFi Status");
			Serial.println("5. List all WiFi Stations");
			Serial.println("6. Turn off HX711");
			Serial.println("7. EXIT menue");
			state = 3;
			break;

		/* Wait for user choice */
		case 3:
			switch(Serial.read() - 48) // 3+atoi(Serial.readStringUntil('\n').c_str())
			{
				case 1: state = 1; break; // 1. Output current weight readings
				case 2: state = 4; break; // 2. Calibrate Scale
				case 3: state = 5; break; // 3. Change WiFi SSID and Password
				case 4: state = 6; break; // 4. Print WiFi Status
				case 5: state = 7; break; // 5. List all WiFi Stations
				case 6: state = 8; break; // 6. Turn off HX711
				case 7: state = 0;        // 7. EXIT
					Serial.println("Exited Menu. Press 'm' to reopen it.");
					break; 
				default: break;
			}
			break;

		/* 2. Calibrate Scale */
		case 4:
			static uint8_t internalState = 0;
			static uint16_t offset = 0;
			static float divider = 1;

			switch (internalState)
			{
				case 0:
					Serial.println("[Calibration] Please remove all weight from it and place it on an even surface. Press enter to continue...");
					hx711.set_scale();
					internalState++;

				case 1: 
					internalState = 1;
					if(Serial.read() == '\n')
					{
						offset = hx711.read_average(10);
						hx711.set_offset(offset);
						delay(100);
					}
					else
						break;

				case 2:
					Serial.println("[Calibration] Now put a known weight on the scale and enter the weight in gramm or just press enter to continue...");

				case 3: 
					internalState = 3;
					if((Serial.peek() >= '0' && Serial.peek() <= '9') || Serial.peek() == '+' || Serial.peek() == '-')
					{
						int knownReference = Serial.parseInt();
						float measurement = hx711.get_units(10);
						Serial.printf("%.2f / %d\n", measurement, knownReference);
						divider = measurement / (float) knownReference;
						hx711.set_scale(divider);
						Serial.printf("[Calibration] Current measurement %.2fg with %.2f as divider.\n", hx711.get_units(10), divider);
						internalState = 2;
					}
					else if(Serial.read() != '\n')
						break;

				case 4:
					Serial.printf("Calibration done. Offset: %d, Divider: %.2f.\n", offset, divider);
					internalState = 0;
					state = 0;
					break;

				default:
					Serial.printf("[Calibration] Ended up in invalid state (%d). Resetting state machine.\n", internalState);
					break;
			}
			break;

		/* 3. Change Wifi SSID and Password */
		case 5:
		{
			unsigned long timeout = Serial.getTimeout();
			Serial.setTimeout(60000); // Warte 1 Minute auf Input
			while(Serial.available()) Serial.read(); // Make sure Serial is empty

			char wifiSSID[32] = {0};
			char wifiPasswd[64] = {0};

			Serial.println("Please enter the SSID to connect to, followed by a line break: ");
			serialReadLine(wifiSSID, 32);
			if(strlen(wifiSSID) < 1)
			{
				Serial.println("Abort, no SSID was entered.");
				break;
			}

			Serial.printf("Please enter the password for %s, followed by a line break: \n", wifiSSID);
			serialReadLine(wifiPasswd, 64);

			WiFi.disconnect();

			Serial.printf("Connection to %s with password ", wifiSSID);
			for(int i=0; i<strlen(wifiPasswd); i++)
				Serial.print("*");
			Serial.println(".");
			
			delay(100);
			Serial.setTimeout(timeout);
			WiFi.begin(wifiSSID, wifiPasswd);
			state = 0;

			break;
		}

		/* 4. Print WiFi status */
		case 6:
			Serial.print("Status: ");
			printWiFiStatus();
			Serial.print("IPv4 address: ");
			Serial.println(WiFi.localIP());
			Serial.print("IPv6 address: ");
			Serial.println(WiFi.localIPv6());
			Serial.print("MAC address: ");
			Serial.println(WiFi.macAddress());
			state = 0;
			break;

		/* 5. List all WiFi Stations */
		case 7:
		{
			WiFi.disconnect();
			Serial.println("Scanning...");
			int n = WiFi.scanNetworks();
			if(n==0)
				Serial.println("No WiFi APs found");
			else
			{
				for(int16_t i=0; i<n; ++i)
					Serial.println(WiFi.SSID(i));
			}
			Serial.println();
			state = 0;
			break;
		}

		/* 6. Turn off HX711 */
		case 8:
			Serial.println("Turning off HX711");
			hx711.power_down();
			state = 0;
			break;

		default:
			digitalWrite(LED_BUILTIN, LOW);
			Serial.printf("SmartScale entered an invalid state (%d). Please reset the device!\n", state);
			delay(1000);
			ESP.deepSleep(0);
			break;
	}

	/*static boolean newDataReady = 0;
	const int serialPrintInterval = 1000; //increase value to slow down serial print activity

	// check for new data/start next conversion:
	if (hx711.update())
		newDataReady = true;

	// get smoothed value from the dataset:
	if (newDataReady)
	{
		if (millis() > t + serialPrintInterval)
		{
			float i = hx711.getData();
			Serial.print("Load_cell output val: ");
			Serial.println(i);
			newDataReady = 0;
			t = millis();
		}
	}

	// receive command from serial terminal, send 't' to initiate tare operation:
	if (Serial.available() > 0)
	{
		char inByte = Serial.read();
		if (inByte == 't')
			hx711.tareNoDelay();
	}

	// check if last tare operation is complete:
	if (hx711.getTareStatus() == true)
	{
		Serial.println("Tare complete");
	}*/
}

void printWiFiStatus(bool verbose)
{
	verbose ? Serial.print("[WiFi] ") : verbose; 
	switch (WiFi.status())
	{
		case WL_NO_SHIELD:
			Serial.println("No shield"); break;
		case WL_IDLE_STATUS:
			Serial.println("Idle status"); break;
		case WL_NO_SSID_AVAIL:
			Serial.println("SSID not reachable"); break;
		case WL_SCAN_COMPLETED:
			Serial.println("Scan completed"); break;
		case WL_CONNECTED:
			verbose ? Serial.printf("Connected. IPv4: %s \n", WiFi.localIP().toString().c_str()) : Serial.println("Connected"); break;
		case WL_CONNECT_FAILED:
			Serial.println("Connection failed after several attempts"); break;
		case WL_CONNECTION_LOST:
			Serial.println("Connection lost"); break;
		case WL_DISCONNECTED:
			Serial.println("Disconnected"); break;
		default:
			break;
	}
}

int serialTimedRead()
{
    int c;
    static unsigned long _startMillis = millis();
    do {
        c = Serial.read();
        if(c >= 0) {
            return c;
        }
    } while(millis() - _startMillis < Serial.getTimeout());
    return -1;     // -1 indicates timeout
}

int serialTimedPeak()
{
    int c;
    static unsigned long _startMillis = millis();
    do {
        c = Serial.peek();
        if(c >= 0) {
            return c;
        }
    } while(millis() - _startMillis < Serial.getTimeout());
    return -1;     // -1 indicates timeout
}

size_t serialReadLine(char *buffer, size_t length)
{
    if(length < 1) {
        return 0;
    }
    size_t index = 0;
    while(index < length) {
        int c = serialTimedRead();
        if(c < 0 || c == '\n') {
            break;
        } else if(c == '\r' && serialTimedPeak() == '\n') {
			Serial.read(); break;
		}
        *buffer++ = (char) c;
        index++;
    }
    return index; // return number of characters, not including null terminator
}