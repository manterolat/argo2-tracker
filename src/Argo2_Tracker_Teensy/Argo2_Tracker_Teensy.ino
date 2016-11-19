/* Argo 2 Tracker Teensy
 *
 * Tomas Manterola
 *
 * Sources:
 * https://adafruit.com/
 * https://stackoverflow.com/
 * https://ukhas.org.uk/
 *
 * Tracker program to run on a Teensy 3.1/3.2 for HAB use.
 * Transmits data including: Internal/External Temperature, Pressure and Humidity.
 * Uses RFM96W LoRa transciever to send telemetry data back to Ground Station.
 * Uses a U-Blox NEO-M8N GPS module in order to track location and altitude.
 *
 * Uses Adafruit libraries for the BME280 and MAX31855 sensors
 * Uses Radiohead library for RFM96W radio (requires SPI Transaction version)
 *
 * Sample TX message: ARGO2,10000,22:22:22,-92.1232322,-90.2322323,30000,-12.0,12.0,32.4,-20.25,-10.1,300.0,56.4,4.32,10,5,1*b762;-50
 * Sample RX message: 1,0*BEEF
 */

// Capsule Callsign
#define CALLSIGN "CHANGE_ME"

// Enable/Disable debug messages
// Source: http://arduino.stackexchange.com/questions/9857/can-i-make-the-arduino-ignore-serial-print
#define DEBUG

#ifdef DEBUG
	#define Sprintln(a) (Serial.println(a))
	#define Sprint(a) (Serial.print(a))
#else
	#define Sprintln(a)
	#define Sprint(a)
#endif

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MAX31855.h>
#include <RH_RF95.h>
#include <TinyGPS++.h>


/*** Configuration of Sensors, GPS and Radio ***/

// Indicator LEDs
#define LED 13

// Battery voltage pin
#define BATT A7

// Buzzer pin
#define BUZZ 8

// Adafruit BME280 Sensor - Uses I2C
#define BME_CS A3
Adafruit_BME280 bme;

// Adafruit MAX31855K Thermocouple Amplifier - Uses SPI
#define MAX_CS A1
//Adafruit_MAX31855 thermocouple(MAX_CS);

// RFM96W Transceiver - uses propietary LoRa radio protocol and transmits between 410.000 and 525.000 MHz
// Uses SPI - RFM96W Chip Select and Interrupt pins
#define RFM_CS A2
#define RFM_INT A0
RH_RF95 rf95(RFM_CS, RFM_INT);

// U-Blox NEO-M8N GPS - uses Serial1
// U-Blox GPS data parser - TinyGPSPlus
TinyGPSPlus gpsParser;


/*** Constants and U-Blox Commands ***/

// States
#define STATE_STANDBY		0
#define STATE_RISING		1
#define STATE_FALLING_HIGH	2
#define STATE_FALLING_LOW	3
#define STATE_LANDING		4

// Commands that can be sent from ground
#define COMMAND_ID_TX_POWER		0
#define COMMAND_ID_GPS_MODE		1
#define COMMAND_ID_GPS_POWER	2
#define COMMAND_ID_BUZZER		3
#define COMMAND_ID_MODE			4

#define COMMAND_VALUE_GPS_MODE_PED 0
#define COMMAND_VALUE_GPS_MODE_AIR 1

#define COMMAND_VALUE_GPS_POWER_MAX 0
#define COMMAND_VALUE_GPS_POWER_SAV 1

#define COMMAND_VALUE_BUZZER_OFF 0
#define COMMAND_VALUE_BUZZER_ON 1

// U-Blox Command: Disable GPGLL NMEA Sentence
const uint8_t GPS_DISABLE_GPGLL[] {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x2B
};

// U-Blox Command: Disable GPGSA NMEA Sentence
const uint8_t GPS_DISABLE_GPGSA[] = {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x32
};

// U-Blox Command: Disable GPGSV NMEA Sentence
const uint8_t GPS_DISABLE_GPGSV[] = {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x39
};

// U-Blox Command: Set Navigation Mode to 'Airborne < 1G' (Large Deviation)
// For altitudes > 9 km
const uint8_t GPS_MODE_AIRBORNE[] = {
	0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
	0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC
};

// U-Blox Command: Set Navigation Mode to 'Pedestrian' (Small Deviation)
// For altitudes < 9 km
const uint8_t GPS_MODE_PEDESTRIAN[] = {
	0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
	0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x76
};

// U-Blox Command: Set to Power Saving Mode
// This mode requires a GPS Fix with >5 satellites (otherwise stability issues may arise)
const uint8_t GPS_POWER_SAVING[] = {
	0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92
};

// U-Blox Command: Set to Max Performance Mode (default)
const uint8_t GPS_POWER_PERFORMANCE[] = {
	0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x00, 0x21, 0x91
};

const uint8_t spaceMsg[] = "";


/*** Global Variables ***/

// Should Buzzer be on
bool buzzerOn = false;
bool buzzerState = false;

// Used to calculate and store vertical speed of capsule
double lastAltitude = 0;
double verticalSpeed = 0;

// Used to confirm that a message was received from ground by capsule
// Shows # of messages received since last transmission (reset to 0 after transmitting)
uint8_t rxACK = 0;

// TX Power: from 5 - 23 (measured in dBm)
uint8_t txPower = 23;

// Buffers to store messages to be received/transmitted (max length is 255 - 4 = 251)
uint8_t rxMsg[RH_RF95_MAX_MESSAGE_LEN];
uint8_t txMsg[RH_RF95_MAX_MESSAGE_LEN];

// Used to control when data is collected/transmitted
// Keeps track of the time in seconds (0 - 59) and minutes (0 - 59)
uint8_t currTimeSec = -1;
uint8_t currTimeMin = -1;
uint8_t currTimeHour = -1;

// Assures that a message hasn't already been sent in the same second
uint8_t lastTimeSec = -1;
uint8_t lastTimeMin = -1;

// Interval of seconds after which a message should be sent
uint8_t interval = 30;

// Buzzer interval (milliseconds), period (milliseconds) and last time on (milliseconds)
uint32_t buzzInterval = 5000;
uint32_t buzzPeriod = 1000;
uint32_t buzzLastOn = 0;

// Sentence ID (number of sentences sent)
uint32_t sentID = 1;

// Save GPS status
uint8_t gpsMode = 0;
uint8_t gpsPower = 0;

// Status - Shows information about program
uint8_t status[7];

// State - Current mode/state of tracker (based on altitude and vertical speed)
uint8_t state = 0;


/*** Functions ***/

// Setup pins, sensors and radio
// Runs only on startup
void setup() {
	#ifdef DEBUG
	// Start serial communication with computer for debugging
	Serial.begin(9600);

	// Wait until computer catches up connecting to Serial
	delay(5000);
	#endif

	// Configure LED, Buzzer and CS pin to OUTPUT
	pinMode(LED, OUTPUT);
	pinMode(BUZZ, OUTPUT);
	pinMode(MAX_CS, OUTPUT);
	pinMode(RFM_CS, OUTPUT);

	// Start serial communication with U-Blox GPS
	Serial1.begin(9600);

	// Set U-Blox Nav Mode to 'Airborne < 1G' and Max Power (retry until correct ACK received)
	Sprint("Setting GPS to Flight Mode...\t");
	setGPSMode(COMMAND_VALUE_GPS_MODE_AIR);
	setGPSPower(COMMAND_VALUE_GPS_POWER_MAX);
	Sprintln("OK!");

	// Disable unnecessary NMEA sentences being sent by U-Blox GPS
	Sprint("Disabling GPS sentences...\t");

	bool gpsSetSuccess = false;
	while (!gpsSetSuccess) {
		sendUBX(GPS_DISABLE_GPGLL, sizeof(GPS_DISABLE_GPGLL) / sizeof(uint8_t));
		gpsSetSuccess = getUBX_ACK(GPS_DISABLE_GPGLL);
	}

	gpsSetSuccess = false;
	while (!gpsSetSuccess) {
		sendUBX(GPS_DISABLE_GPGSA, sizeof(GPS_DISABLE_GPGSA) / sizeof(uint8_t));
		gpsSetSuccess = getUBX_ACK(GPS_DISABLE_GPGSA);
	}

	gpsSetSuccess = false;
	while (!gpsSetSuccess) {
		sendUBX(GPS_DISABLE_GPGSV, sizeof(GPS_DISABLE_GPGSV) / sizeof(uint8_t));
		gpsSetSuccess = getUBX_ACK(GPS_DISABLE_GPGSV);
	}

	Sprintln("OK!");

	// Initialize BME280
	Sprint("Starting BME280 Sensors...\t");
	if (!bme.begin()) {
		Sprintln("ERROR");

		// If it can't be initialized, stop the program and blink every 1s
		while (true) {
			digitalWrite(LED, !digitalRead(LED));
			digitalWrite(BUZZ, !digitalRead(BUZZ));
			delay(1000);
		}
	}
	Sprintln("OK!");

	// NOT USED
	/*// Initialize Micro-SD Card Reader
	Sprint("Starting SD Card Reader...\t");
	if (!SD.begin(SD_CS)) {
		Sprintln("ERROR");

		// If it can't be initialized, stop the program and blink every 0.25s
		while (true) {
			digitalWrite(LED, !digitalRead(LED));
			delay(250);
		}
	}
	Sprintln("OK!");*/

	// Initialize RFM96W - if there's an error retry
	while (!rf95.init()) {
		Sprintln("RFM96W Initialization Failed!");
		digitalWrite(BUZZ, HIGH);
		delay(1000);
		digitalWrite(BUZZ, LOW);
	}

	// Configure RFM96W and set it to receiver mode so that it can receive messages
	configureRFM96W();
	rf95.setModeRx();

	// Wait until U-Blox GPS has a fix (5 or more satellites) before starting main program
	Sprint("Waiting for GPS Fix...\t\t");
	while (gpsParser.satellites.value() < 5) {
		while (Serial1.available()) {
			gpsParser.encode(Serial1.read());
		}
	}
	Sprintln("OK!");

	// Save 'lastAltitude' for vertical velocity calculation
	lastAltitude = gpsParser.altitude.meters();
}


// Manage reception, transmission, data logging and settings - Repeat forever
void loop() {

	// Update current time in seconds (0 - 59) and minutes (0 - 59)
	currTimeMin = gpsParser.time.minute();
	currTimeSec = gpsParser.time.second();


	/*** Status / State ***/

	// State: current state of tracker
	//	0. Standby (on ground)
	//		TX_POWER:	23
	//		GPS_MODE:	Airborne
	//		GPS_POWER:	MAX
	//		BUZZER:		OFF
	//
	//	1. Rising	(vertical speed > 1.0 m/s and altitude > 500 m)
	//		TX_POWER:	23
	//		GPS_MODE:	Airborne
	//		GPS_POWER:	MAX
	//		BUZZER:		OFF
	//
	//	2. Falling (vertical speed < 1.0)
	//		TX_POWER:	23
	//		GPS_MODE:	Airborne
	//		GPS_POWER:	MAX
	//		BUZZER:		OFF
	//
	//	3. Falling, Low ALitude (vertical speed < 1.0 and altitude < 9,000 m)
	//		TX_POWER:	23
	//		GPS_MODE:	Pedestrian
	//		GPS_POWER:	MAX
	//		BUZZER:		OFF
	//
	//	4. Landing (vertical speed < 1.0 and altitude < 2,000 m)
	//		TX_POWER:	23
	//		GPS_MODE:	Pedestrian
	//		GPS_POWER:	MAX
	//		BUZZER:		ON

	switch (state) {

		// On ground. Check if we have launched (going up).
		case STATE_STANDBY:

			// If vertical speed is greater than 1 m/s and altitude is greater than 500 m, switch to RISING state
			if (verticalSpeed > 1.0 && gpsParser.altitude.meters() > 500.0) {
				state = STATE_RISING;

				txPower = 23;

				if (gpsMode != COMMAND_VALUE_GPS_MODE_AIR) {
					setGPSPower(COMMAND_VALUE_GPS_MODE_AIR);
				}

				if (gpsPower != COMMAND_VALUE_GPS_POWER_MAX) {
					setGPSMode(COMMAND_VALUE_GPS_POWER_MAX);
				}

				buzzerOn = false;
			}

			break;

		// Rising. Check if falling.
		case STATE_RISING:

			// If vertical speed is less than 1 m/s, switch to FALLING_HIGH state.
			if (verticalSpeed < 1.0) {
				state = STATE_FALLING_HIGH;

				txPower = 23;

				if (gpsMode != COMMAND_VALUE_GPS_MODE_AIR) {
					setGPSPower(COMMAND_VALUE_GPS_MODE_AIR);
				}

				if (gpsPower != COMMAND_VALUE_GPS_POWER_MAX) {
					setGPSMode(COMMAND_VALUE_GPS_POWER_MAX);
				}

				buzzerOn = false;
			}

			break;

		// Falling (altitude higher than 9,000 m). Check if we are below 9,000 m.
		case STATE_FALLING_HIGH:

			// If vertical speed is less than 1 m/s and altitude is less than 9,000 m, switch to FALLING_LOW state.
			if (verticalSpeed < 1.0 && gpsParser.altitude.meters() < 9000.0) {
				state = STATE_FALLING_LOW;

				txPower = 23;

				if (gpsMode != COMMAND_VALUE_GPS_MODE_AIR) {
					setGPSPower(COMMAND_VALUE_GPS_MODE_AIR);
				}

				if (gpsPower != COMMAND_VALUE_GPS_POWER_MAX) {
					setGPSMode(COMMAND_VALUE_GPS_POWER_MAX);
				}

				buzzerOn = false;
			}

			break;

		// Falling (altitude lower than 9,000 m). Check if we are below 2,000 m.
		case STATE_FALLING_LOW:

			// If altitude is lower than 2,000 m, switch to LANDING state
			if (gpsParser.altitude.meters() < 2000.0) {
				state = STATE_LANDING;

				txPower = 23;

				if (gpsMode != COMMAND_VALUE_GPS_MODE_PED) {
					setGPSPower(COMMAND_VALUE_GPS_MODE_PED);
				}

				if (gpsPower != COMMAND_VALUE_GPS_POWER_MAX) {
					setGPSMode(COMMAND_VALUE_GPS_POWER_MAX);
				}

				buzzerOn = true;
			}

			break;

		// Landing (altitude lower than 2,000).
		case STATE_LANDING:
			if (verticalSpeed < 0.2) {

			}

			break;
	}

	// Status - shows current functioning of the tracker
	// [STATE][TX_POWER][GPS_MODE][GPS_POWER][BUZZER]
	// Example: 113100 -> State: 1, TX Power: 13, GPS Mode: 1, GPS Power: 0, Buzzer: 1
	sprintf((char*)status, "%u%02u%u%u%d", state, txPower, gpsMode, gpsPower, buzzerOn);


	/*** Receive Messages ***/

	// Check for messages from ground - Format of message: [MSG]*[CHKSUM]
	if (rf95.available()) {
		uint8_t len = sizeof(rxMsg);

		// Receive data and check for errors ('rf95.recv(data, &len)' returns 0 if error occurs)
		if (rf95.recv(rxMsg, &len)) {
			Sprintln("Got new message!");

			do {

				/*** Parse Message ***/

				// Check that message is formatted correctly (lenght > 5 and contains '*')
				if (strlen((char*)rxMsg) > 5 && strchr((char*)rxMsg, '*') != NULL) {

					char checksumRX[5];
					char checksumCalc[5];

					// Get checksum from last part of message
					strncpy(checksumRX, ((char*)rxMsg + strlen((char*)rxMsg) - 4), 5);

					// Calculate checksum from message (removing last 5 characters e.g: '*BEEF')
					snprintf(checksumCalc, sizeof(checksumCalc), "%04X", calc_CRC16(rxMsg, strlen((char*)rxMsg) - 5));

					// Compare received checksum with checksum calculated from message
					if (strcmp(checksumRX, checksumCalc) == 0) {
						Sprintln("Checksum is valid!");

						// Acknowledge received message
						rxACK++;

						// Get ID and Value from message
						char msgID[2];
						char msgValue[3];

						// Incoming messages can either be 3 or 4 characters in length (plus checksum: 5)
						if (strlen((char*)rxMsg) == 3 + 5) {
							strncpy(msgID, (char*)rxMsg, 1);
							strncpy(msgValue, (char*)rxMsg + 2, 1);

						} else if (strlen((char*)rxMsg) == 4 + 5) {
							strncpy(msgID, (char*)rxMsg, 1);
							strncpy(msgValue, (char*)rxMsg + 2, 2);
						} else {
							// Bad message - don't count it and exit
							rxACK--;
							break;
						}
						Sprint("ID: ");
						Sprintln(msgID);
						Sprint("Value: ");
						Sprintln(msgValue);

						// Convert string to int
						uint8_t ID = atoi(msgID);
						uint8_t value = atoi(msgValue);

						// Find and run message
						switch (ID) {

							// Command to change TX Power to 'value'
							case COMMAND_ID_TX_POWER:
								if (value >= 5 || value <= 23) {
									txPower = value;

									Sprint("Setting TX_POWER to: ");
									Sprintln(value);
								}
								break;


							case COMMAND_ID_GPS_MODE:
								if (value == COMMAND_VALUE_GPS_MODE_PED) {
									setGPSMode(COMMAND_VALUE_GPS_MODE_PED);

								} else if (value == COMMAND_VALUE_GPS_MODE_AIR) {
									setGPSMode(COMMAND_VALUE_GPS_MODE_AIR);

								}
								break;


							case COMMAND_ID_GPS_POWER:
								if (value == COMMAND_VALUE_GPS_POWER_MAX) {
									setGPSPower(COMMAND_VALUE_GPS_POWER_MAX);

								} else if (value == COMMAND_VALUE_GPS_POWER_SAV) {
									setGPSPower(COMMAND_VALUE_GPS_POWER_SAV);

								}
								break;


							case COMMAND_ID_BUZZER:
								if (value == COMMAND_VALUE_BUZZER_OFF) {
									buzzerOn = false;

								} else if (value == COMMAND_VALUE_BUZZER_ON) {
									buzzerOn = true;

								}
								break;

							case COMMAND_ID_MODE:

								switch (value) {

									case STATE_STANDBY:
										state = STATE_STANDBY;
										break;

									case STATE_RISING:
										state = STATE_RISING;
										break;

									case STATE_FALLING_HIGH:
										state = STATE_FALLING_HIGH;
										break;

									case STATE_FALLING_LOW:
										state = STATE_FALLING_LOW;
										break;

									case STATE_LANDING:
										state = STATE_LANDING;
										break;
										
								}

							default:
								// Not a valid ID - revoke ACK
								rxACK--;
								break;
						}

					}
				}

			} while (false);

		} else {
			Sprintln("Error receiving message!");
		}
	}


	/*** Get GPS Data ***/

	// Read data from U-Blox GPS and pass it to parser
	while (Serial1.available()) {
		gpsParser.encode(Serial1.read());
	}


	/*** Save and Transmit Data and Location ***/

	// Check if it's time to collect/send data
	// If time is on interval and not the same time (sec or min different?) as last transmission, then collect/send
	if (currTimeSec % interval == 0	&& (currTimeSec != lastTimeSec || currTimeMin != lastTimeMin)) {
		Sprintln("Sending Data!");
		// Reset interval timer
		lastTimeSec = currTimeSec;
		lastTimeMin = currTimeMin;


		/*** Data ***/

		// Get data from sensors
		double extTemperature	= thermocouple.readCelsius();	// External Temperature in °C
		double intTemperature	= thermocouple.readInternal();	// Internal Temperature in °C
		double pressure			= bme.readPressure() / 100.0F;	// Pressure in hPa
		double humidity			= bme.readHumidity();			// Humidity in %

		// Get battery voltage from BATT pin
		double voltage = analogRead(BATT) * 2.0 * 3.3 / 1024.0;	// Battery Voltage in V
		
		// Make sure thermocouple data is valid. If not set to predefined error value (-0.01)
		if (isnan(extTemperature)) {
			extTemperature = -0.01;
		}

		/*** Data Processing ***/

		// Move data to strings (for easy formatting)
		char extT[8];
		char intT[8];
		char pres[8];
		char humi[8];
		char vbat[8];

		/*dtostrf(extTemperature, 4, 2, extT);
		dtostrf(intTemperature, 3, 1, intT);
		dtostrf(pressure, 3, 1, pres);
		dtostrf(humidity, 3, 1, humi);
		dtostrf(voltage, 4, 2, vbat);*/

		sprintf(extT, "%.2f", extTemperature);
		sprintf(intT, "%.1f", intTemperature);
		sprintf(pres, "%.1f", pressure);
		sprintf(humi, "%.1f", humidity);
		sprintf(vbat, "%.2f", voltage);


		/*** GPS Data ***/

		char time[16];
		char latitude[16];
		char longitude[16];
		char altitude[16];
		char vSpeed[16];
		char speed[16];
		char course[16];
		char satellites[16];

		// Calculate Vertical Speed using altitude values (difference in altitude over difference in time)
		verticalSpeed = (gpsParser.altitude.meters() - lastAltitude) / (double)interval;

		// Get GPS Data from TinyGPS parser
		sprintf(time, "%02u:%02u:%02u",
				gpsParser.time.hour(),
				gpsParser.time.minute(),
				gpsParser.time.second()
		);

		/*dtostrf(gpsParser.location.lat(), 9, 7, latitude);
		dtostrf(gpsParser.location.lng(), 9, 7, longitude);
		dtostrf(gpsParser.altitude.meters(), 3, 1, altitude);
		dtostrf(verticalSpeed, 3, 1, vSpeed);
		dtostrf(gpsParser.speed.mps(), 3, 1, speed);
		dtostrf(gpsParser.course.deg(), 3, 1, course);*/
		
		sprintf(latitude, 	"%.7f", gpsParser.location.lat());
		sprintf(longitude, 	"%.7f", gpsParser.location.lng());
		sprintf(altitude, 	"%.1f", gpsParser.altitude.meters());
		sprintf(vSpeed, 	"%.1f", verticalSpeed);
		sprintf(speed, 		"%.1f", gpsParser.speed.mps());
		sprintf(course, 	"%.1f", gpsParser.course.deg());
		
		sprintf(satellites, "%lu", 	gpsParser.satellites.value());


		/*** Arrange datastring ***/

		// Arrange datastring to transmit/save
		sprintf((char*)txMsg,
			"%s,%lu,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%u",
			CALLSIGN,												// Callsign
			sentID,													// Sentence ID
			time,													// Time
			latitude,												// Latitude
			longitude,												// Longitude
			altitude,												// Altitude
			vSpeed,													// Vertical Speed
			speed,													// Speed
			course,													// Course
			extT,													// External Temperature
			intT,													// Internal Temperature
			pres,													// Pressure
			humi,													// Humidity
			vbat,													// Battery Voltage
			satellites,												// Satellites
			status,													// Status
			rxACK													// ACK
		);

		// Calculate and add checksum to message
		char checksum_str[8];
		snprintf(checksum_str, sizeof(checksum_str), "*%04X", calc_CRC16(txMsg, strlen((char*)txMsg)));
		strcat((char*)txMsg, checksum_str);


		/*** Log Data - NOT USED ***/
		/*
		// Write GPS data to "gps.log"
		File gpsFile = SD.open("gps.log", FILE_WRITE);

		gpsFile.print(time);
		gpsFile.print(",");
		gpsFile.print(latitude);
		gpsFile.print(",");
		gpsFile.print(longitude);
		gpsFile.print(",");
		gpsFile.print(altitude);
		gpsFile.print(",");
		gpsFile.print(vSpeed);
		gpsFile.print(",");
		gpsFile.print(speed);
		gpsFile.print(",");
		gpsFile.print(course);
		gpsFile.print(",");
		gpsFile.print(satellites);
		gpsFile.println();

		gpsFile.close();

		// Write sensor data to "data.log"
		File dataFile = SD.open("data.log", FILE_WRITE);

		dataFile.print(time);
		dataFile.print(",");
		dataFile.print(extT);
		dataFile.print(",");
		dataFile.print(intT);
		dataFile.print(",");
		dataFile.print(pres);
		dataFile.print(",");
		dataFile.print(humi);
		dataFile.print(",");
		dataFile.print(vbat);
		dataFile.println();

		dataFile.close();

		// Write latest sentence to "sentence.log"
		File sentFile = SD.open("sentence.log", FILE_WRITE);

		sentFile.println((char*)txMsg);

		sentFile.close();*/


		/*** Send Message ***/

		// Send data
		sendData(txMsg);

		// Update sentID
		sentID++;
		
		// Update lastAltitude
		lastAltitude = gpsParser.altitude.meters();
		
		// Reset rxACK
		rxACK = 0;
	}


	/*** Buzzer Timing ***/

	// Turn Buzzer On/Off
	// Buzzer is turned on if 'buzzerOn' is true every 5 seconds (for a duration of 5 seconds)
	if (buzzerOn) {
		if (currTimeSec % 10 < 5) {
			digitalWrite(BUZZ, HIGH);
		} else {
			digitalWrite(BUZZ, LOW);
		}
	} else {
		digitalWrite(BUZZ, LOW);
	}

}


//	RFM96W Transceiver Configuration (Registers: 0x1D, 0x1E, 0x26)
//
//	Bandwith (BW): 125 kHz			= 7
//	Coding Rate (CR): 4/8			= 4
//	Mode: Explicit					= 0
//	Spreading Factor (SF): 4096		= 12
//	LowDataRateOptimization: ON		= 1
//
//	Preamble Length					= 8
//	Frequency						= 434.000 MHz

void configureRFM96W() {
	const RH_RF95::ModemConfig cfg = {
		// Register 0x1D:
		// BW		  CR		Mode
		(7 << 4) | (4 << 1) | (0 << 0),

		// Register 0x1E:
		//	SF
		(12 << 4),

		// Register 0x26:
		// Low Data Rate Optimization
		(1 << 3)
	};
	rf95.setModemRegisters(&cfg);

	rf95.setPreambleLength(8);
	rf95.setFrequency(434.000);
}


// Send data using RFM96W LoRa Transceiver
// Uses 'txPower' to set transmission power
void sendData(uint8_t msg[]) {
	// Set TX Power to value received from GroundStation
	rf95.setTxPower(txPower);

	// Send message
	// Length doesn't include null-character (string terminator), so add 1
	rf95.send(msg, strlen((char*)msg) + 1);
	
	// Wait until packet is completely sent
	rf95.waitPacketSent();
	
	// Set RFM96W back to receiver mode so that it can receive any new messages
	rf95.setModeRx();
}


// Calculate CRC16-CCITT checksum
// Source: https://stackoverflow.com/questions/10564491/function-to-calculate-a-crc16-checksum
uint16_t calc_CRC16(const uint8_t* data_p, uint8_t length){
	uint8_t x;
	uint16_t crc = 0xFFFF;

	while (length--){
		x = crc >> 8 ^ *data_p++;
		x ^= x>>4;
		crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
	}
	return crc;
}


// Set GPS Navigation Mode (and verify)
void setGPSMode(uint8_t MODE) {
	bool gpsSetSuccess = false;

	if (MODE == COMMAND_VALUE_GPS_MODE_PED) {
		while (!gpsSetSuccess) {
			sendUBX(GPS_MODE_PEDESTRIAN, sizeof(GPS_MODE_PEDESTRIAN) / sizeof(uint8_t));
			gpsSetSuccess = getUBX_ACK(GPS_MODE_PEDESTRIAN);
		}
	} else if(MODE == COMMAND_VALUE_GPS_MODE_AIR) {
		while (!gpsSetSuccess) {
			sendUBX(GPS_MODE_AIRBORNE, sizeof(GPS_MODE_AIRBORNE) / sizeof(uint8_t));
			gpsSetSuccess = getUBX_ACK(GPS_MODE_AIRBORNE);
		}
	}
	gpsMode = MODE;
}


// Set GPS Power Mode (and verify)
// Setting Power Saving mode can block tracker, so a counter is used to avoid this
void setGPSPower(uint8_t POWER) {
	bool gpsSetSuccess = false;
	uint8_t counter = 0;

	if (POWER == COMMAND_VALUE_GPS_POWER_MAX) {
		while (!gpsSetSuccess) {
			sendUBX(GPS_POWER_PERFORMANCE, sizeof(GPS_POWER_PERFORMANCE) / sizeof(uint8_t));
			gpsSetSuccess = getUBX_ACK(GPS_POWER_PERFORMANCE);
		}
	} else if(POWER == COMMAND_VALUE_GPS_POWER_SAV) {
		while (!gpsSetSuccess && counter < 5) {
			sendUBX(GPS_POWER_SAVING, sizeof(GPS_POWER_SAVING) / sizeof(uint8_t));
			gpsSetSuccess = getUBX_ACK(GPS_POWER_SAVING);
			counter++;
		}
	}
	
	if (counter != 5)
		gpsPower = POWER;
}


// Send command to U-Blox GPS on Serial1
// Source: https://ukhas.org.uk/guides:ublox6
void sendUBX(const uint8_t *MSG, uint8_t len) {
	for (int i = 0; i < len; i++) {
		Serial1.write(MSG[i]);
	}
	Serial1.println();
}


// Verify that message sent to U-Blox was received and acknowledged
// Source: https://ukhas.org.uk/guides:ublox6
bool getUBX_ACK(const uint8_t *MSG) {
	uint8_t b;
	uint8_t ackByteID = 0;
	uint8_t ackPacket[10];
	unsigned long startTime = millis();

	ackPacket[0] = 0xB5;	// header
	ackPacket[1] = 0x62;	// header
	ackPacket[2] = 0x05;	// class
	ackPacket[3] = 0x01;	// id
	ackPacket[4] = 0x02;	// length
	ackPacket[5] = 0x00;
	ackPacket[6] = MSG[2];	// ACK class
	ackPacket[7] = MSG[3];	// ACK id
	ackPacket[8] = 0;		// CK_A
	ackPacket[9] = 0;		// CK_B

	// Calculate the checksums
	for (uint8_t i = 2; i < 8; i++) {
		ackPacket[8] = ackPacket[8] + ackPacket[i];
		ackPacket[9] = ackPacket[9] + ackPacket[8];
	}

	while (true) {
		// Test for success
		if (ackByteID > 9) {
			// All packets in order!
			return true;
		}

		// Timeout if no valid response in 3 seconds
		if (millis() - startTime > 3000) {
			return false;
		}

		// Make sure data is available to read
		if (Serial1.available()) {
			b = Serial1.read();

			// Check that bytes arrive in sequence as per expected ACK packet
			if (b == ackPacket[ackByteID]) {
				ackByteID++;
			}
			// Reset and look again, invalid order
			else {
				ackByteID = 0;
			}
		}
	}
}


// Parses a double value into a string
// AVR Arduino and the Cortex M0 can't do this with 'snprintf' and need this function
/*char *dtostrf(double val, int width, unsigned int prec, char *sout) {
	int decpt, sign, reqd, pad;
	const char *s, *e;
	char *p;
	
	s = fcvt(val, prec, &decpt, &sign);
	
	if (prec == 0 && decpt == 0) {
		s = (*s < '5') ? "0" : "1";
		reqd = 1;
	} else {
		reqd = strlen(s);
		if (reqd > decpt) reqd++;
		if (decpt == 0) reqd++;
	}
	
	if (sign) reqd++;
	
	p = sout;
	e = p + reqd;
	pad = width - reqd;
	
	if (pad > 0) {
		e += pad;
		while (pad-- > 0) *p++ = ' ';
	}
	
	if (sign) *p++ = '-';
	
	if (decpt <= 0 && prec > 0) {
		*p++ = '0';
		*p++ = '.';
		e++;
		while ( decpt < 0 ) {
			decpt++;
			*p++ = '0';
		}
	}
	
	while (p < e) {
		*p++ = *s++;
		if (p == e) break;
		if (--decpt == 0) *p++ = '.';
	}
	if (width < 0) {
		pad = (reqd + width) * -1;
		while (pad-- > 0) *p++ = ' ';
	}
	
	*p = 0;
	
	return sout;
}*/


/*// Measure available RAM (out of 32K)
extern "C" char *sbrk(int i);
int freeRam () {
	char stack_dummy = 0;
	return &stack_dummy - sbrk(0);
}*/