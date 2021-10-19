#include <Arduino.h>
#include <HX711.h>
#include <EEPROM.h>

#define HX711_DOUT 25
#define HX711_SCK 26

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

HX711 hx711;

uint8_t state = 0;

float calibrationValue = 1;

unsigned long t = 0;

bool ledState = false;

void setup() 
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	Serial.begin(115200);
	delay(100);
	Serial.println();
	Serial.println(" --- Smart Kitchen Scale ---");

	/* --- Init HX711 --- */
	hx711.begin(HX711_DOUT, HX711_SCK);
	EEPROM.begin(512);
	//EEPROM.get(calVal_eepromAdress, calibrationValue);

	//hx711.setCalFactor(calibrationValue);
	//Serial.printf("Initialized HX711 with calibration factor %.2f\n", calibrationValue);

	Serial.println("To open the configuration menu send an 'm' over the serial console.");
}

void loop()
{
	digitalWrite(LED_BUILTIN, ledState = !ledState);

	switch (state)
	{
		// Normal operation
		case 1:
			if (hx711.is_ready())
			{
				float val = hx711.get_value(5)/1000;
				Serial.printf("Gewicht: %.2fg\n", val);
				delay(1500);
			}
			else
				Serial.println("Timeout, check MCU => HX711 wiring and pin designations");
			
		case 0:
			switch(Serial.peek())
			{
				case 'm':
					state = 2;
					Serial.read();
					break;

				case 't':
					hx711.tare();
					Serial.println("Tare");
					delay(500);
					Serial.read();
					break;
					
				default:
					break;
			}
			break;
		
		// SmartScale Menu
		case 2:
			Serial.println("Choose one of the following options by entering their number:");
			Serial.println("1. Output current weight readings");
			Serial.println("2. Calibrate Scale");
			Serial.println("3. Change WiFi SSID and Password");
			Serial.println("4. EXIT");
			state = 3;
			break;

		// Wait for user choice	
		case 3:
			switch(Serial.read() - 48) // 3+atoi(Serial.readStringUntil('\n').c_str())
			{
				case 1: state = 1; break; // 1. Output current weight readings
				case 2: state = 4; break; // 2. Calibrate Scale
				case 3: state = 5; break; // 3. Change WiFi SSID and Password
				case 4: state = 0; 
					Serial.println("Exited Menu. Press 'm' to reopen it.");
					break; // 4. EXIT
				default: break;
			}
			break;

		// 2. Calibrate Scale
		case 4:
			state = 0;
			break;

		// 3. Change Wifi SSID and Password
		case 5:
			state = 0;
			break;

		default:
			digitalWrite(LED_BUILTIN, LOW);
			Serial.printf("SmartScale entered an invalid state (%d). Please reset the device!", state);
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