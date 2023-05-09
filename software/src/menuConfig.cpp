#include <EEPROM.h>

#include "menuConfig.hpp"
#include "main.hpp"

// Disable the entire UART config menu, if Serial is disabled
#ifdef ENABLE_ALL_SERIAL

static bool killallhumans = false;

volatile static uint16_t menuState = 0;

#define NUM_STATES 9

void initMenu()
{
	Serial.println("Init Menu");
	menuState = 0;
}

void printMenuInfo()
{
	Serial.println("To open the configuration menu send an 'm' over the serial console.");
	Serial.println("To tare the scale from the command line, send a 't'.");
}

uint16_t waitForInput(uint16_t currState)
{
	switch(Serial.read())
	{
		case 'm':
			return 3;

		case 't':
			tare();
			delay(500);
			break;

		default:
			break;
	}

	return currState;
}

uint16_t printCurrentWeight(uint16_t currState)
{
	if (hx711.is_ready())
	{
		currentWeight = hx711.get_units(5);
		Serial.printf("Gewicht: %.2fg\n", currentWeight);
		delay(1500);
	}
	else
		Serial.println("Timeout, check MCU => HX711 wiring and pin designations");

	return currState;
}

uint16_t printMenu(uint16_t currState)
{
	Serial.println();
	Serial.println("Choose one of the following options by entering their number:");
	Serial.println("1. Output current weight readings");
	Serial.println("2. Calibrate Scale");
	Serial.println("3. Change WiFi SSID and Password");
	Serial.println("4. Print WiFi Status");
	Serial.println("5. List all WiFi Stations");
	Serial.println("6. Turn off HX711");
	Serial.println("7. EXIT menu");
	
	return 4;
}

uint16_t menuWaitForInput(uint16_t currState)
{
	// NOTE: Fast ACII to number conversion by subtracting the offset of '0'
	// 3+atoi(Serial.readStringUntil('\n').c_str())
	uint16_t userInput = Serial.read() - '0';

	const int stateConversion[] = {
		3, 		// 1. Output current weight readings
		5, 		// 2. Calibrate Scale
		6, 		// 3. Change WiFi SSID and Password
		7, 		// 4. Print WiFi Status
		8, 		// 5. List all WiFi Stations
		9, 		// 6. Turn off HX711
		1  		// 7. EXIT
	};

	if(userInput < 7)
		return stateConversion[userInput];
	
	return currState;
}

uint16_t calibrateScale(uint16_t currState)
{
	static uint8_t internalState = 0;
	static long offset = 0;
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

		case 4: {
			if(divider == 0.0f)
			{
				Serial.printf("Calibration failed, divider is Zero! Offset: %ld\n", offset);
				return 1;
			}

			Serial.printf("Calibration done. Offset: %ld, Divider: %.2f.\n", offset, divider);
			EEPROM.writeLong(ADDR_OFFSET, offset);
			EEPROM.writeFloat(ADDR_DIVIDER, divider);
			delay(200); // Neccesary to save
			EEPROM.commit();
			Serial.printf("Values written to EEPROM address %d-%d and %d-%d.\n", ADDR_OFFSET, ADDR_OFFSET+sizeof(offset), ADDR_DIVIDER, ADDR_DIVIDER+sizeof(offset));
			internalState = 0;

			long hx711_offset = 0;
			float hx711_divider = 0.0f;
			EEPROM.get(ADDR_OFFSET, hx711_offset);
			EEPROM.get(ADDR_DIVIDER, hx711_divider);
			Serial.printf("%ld, %.2f\n", hx711_offset, hx711_divider);
			return 1;
		}

		default:
			Serial.printf("[Calibration] Ended up in invalid state (%d). Resetting state machine.\n", internalState);
			return 1;
	}
	return currState;
}

uint16_t changeWifiSettings(uint16_t currState)
{
	unsigned long timeout = Serial.getTimeout();
	Serial.setTimeout(60000);					// Wait 1 minute for input
	while(Serial.available()) Serial.read();	// Make sure Serial is empty

	char wifiSSID[32] = {0};
	char wifiPasswd[64] = {0};

	Serial.println("Please enter the SSID to connect to, followed by a line break: ");
	serialReadLine(wifiSSID, 32);
	if(strlen(wifiSSID) < 1)
	{
		Serial.println("Abort, no SSID was entered.");
		return 1;
	}

	Serial.printf("Please enter the password for %s, followed by a line break: \n", wifiSSID);
	serialReadLine(wifiPasswd, 64);

	WiFi.disconnect();

	Serial.printf("Connecting to %s with password ", wifiSSID);
	for(int i=0; i<strlen(wifiPasswd); i++)
		Serial.print("*");
	Serial.println(".");
	
	delay(100);
	Serial.setTimeout(timeout);
	WiFi.begin(wifiSSID, wifiPasswd);
	return 1;
}

uint16_t printWifiInfo(uint16_t currState)
{
	Serial.print("Status: ");
	printWiFiStatus();
	Serial.print("IPv4 address: ");
	Serial.println(WiFi.localIP());
	Serial.print("IPv6 address: ");
	Serial.println(WiFi.localIPv6());
	Serial.print("MAC address: ");
	Serial.println(WiFi.macAddress());
	return 1;
}

uint16_t printWifiNetworks(uint16_t currState)
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
	return 1;
}

uint16_t disableHX711(uint16_t currState)
{
	Serial.println("Turning off HX711");
	hx711.power_down();
	return 1;
}

uint16_t invalidState(uint16_t currState)
{
	digitalWrite(LED_BUILTIN, LOW);
	Serial.printf("SmartScale entered an invalid state (%d). Please reset the device!\n", currState);
	Serial.flush();
	delay(1000);
	ESP.deepSleep(0);
	return currState;
}

void updateMenu()
{
	switch(menuState)
	{
		case 0:
			menuState = waitForInput(menuState);
			break;

		case 1:
			menuState = closeMenu(menuState);

		case 2:
			menuState = printCurrentWeight(menuState);
			break;

		case 3:
			menuState = printMenu(menuState);
			break;

		case 4:
			menuState = menuWaitForInput(menuState);
			break;

		case 5:
			menuState = calibrateScale(menuState);
			break;

		case 6:
			menuState = changeWifiSettings(menuState);
			break;

		case 7:
			menuState = printWifiInfo(menuState);
			break;
		
		case 8:
			menuState = printWifiNetworks(menuState);
			break;

		case 9:
			menuState = disableHX711(menuState);
			break;
	
		default:
			invalidState(menuState);
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

uint16_t closeMenu(uint16_t currState)
{
	Serial.println("Exited Menu. Press 'm' to reopen it.");
	return 0;
}

void printWiFiStatus(bool verbose)
{
	verbose ? Serial.print("[WiFi] ") : Serial.print(""); 
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

#endif // DISABLE_ALL_SERIAL