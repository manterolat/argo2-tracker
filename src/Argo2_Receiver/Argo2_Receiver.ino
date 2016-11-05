/* Argo 2 Receiver
 * 
 * Tomas Manterola
 * 
 * Adafruit Feather M0 Adalogger Program for interface with RFM96W receiver.
 * Receives and transmits data from and to capsule.
 * GroundStation program must be used on computer connected to the microcontroller.
 */

#include <SPI.h>

#include <RH_RF95.h>


// Indicator LEDs
#define RX_LED 8
#define TX_LED 13


// RFM96W Chip Select and Interrupt pins
#define RFM_CS 17
#define RFM_INT 18

// Configure RFM96W Transceiver - uses propietary LoRa radio protocol and transmits between 410.000 and 525.000 MHz
RH_RF95 rf95(RFM_CS, RFM_INT);


uint8_t tx_power = 20;
uint8_t msg[RH_RF95_MAX_MESSAGE_LEN];


void setup() {
	// Start serial communication with computer
	Serial.begin(9600);
	
	// Configure LEDs to OUTPUT
	pinMode(RX_LED, OUTPUT);
	pinMode(TX_LED, OUTPUT);
	
	// Wait until computer catches up connecting to Serial
	delay(5000);
	
	// Initialize RFM96W - if there's an error retry
	while (!rf95.init()) {
		Serial.println("RFM96W Initialization Failed!");
		delay(1000);
	}
	
	//Configure RFM96W
	configureRFM96W();
	
	// Set RFM96W to receiver mode so that it can receive messages
	rf95.setModeRx();
}


void loop() {
	// Check for messages from capsule and send to Ground Station ([MSG];[RSSI])
	if (rf95.available()) {
		uint8_t data[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(data);
		
		// Receive data and check for errors ('rf95.recv(data, &len)' returns 0 if error occurs)
		if (rf95.recv(data, &len)) {
			// Turn on RX_LED
			digitalWrite(RX_LED, HIGH);
			
			// Send data
			Serial.print((char*)data);
			
			// Add RSSI to message
			Serial.print(";");
			Serial.println(rf95.lastRssi(), DEC);
			
			// Turn off RX_LED
			delay(100);
			digitalWrite(RX_LED, LOW);
			
		} else {
			Serial.println("Error receiving message!");
		}
	}
	
	// Send any messages from Ground Station to capsule ([TX_POWER];[MSG])
	if (Serial.available()) {
		// Read TX Power and message to be transmitted from Serial and save these in 'tx_power' and 'msg' respectively
		tx_power = Serial.readStringUntil(';').toInt();
		Serial.readStringUntil('\n').toCharArray((char*)msg, sizeof(msg));
		
		// Calculate and append checksum
		char checksum_str[8];
		snprintf(checksum_str, sizeof(checksum_str), "*%04X", crc16(msg, strlen((char*)msg)));
		strcat((char*)msg, checksum_str);
		
		// Send data
		sendData(msg);
	}
}


// 	RFM96W Transceiver Configuration (Registers: 0x1D, 0x1E, 0x26)
//
// 	Bandwith (BW): 125 kHz 			= 7
// 	Coding Rate (CR): 4/8 			= 4
// 	Mode: Explicit					= 0
// 	Spreading Factor (SF): 4096		= 12
// 	LowDataRateOptimization: ON 	= 1
//
//	Preamble Length 				= 8
//	Frequency						= 434.000 MHz

void configureRFM96W() {
	const RH_RF95::ModemConfig cfg = {
		// Register 0x1D:
		// BW   	  CR        Mode
		(7 << 4) | (4 << 1) | (0 << 0),
		
		// Register 0x1E:
		//  SF
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
// Uses 'tx_power' to set transmission power
void sendData(uint8_t msg[]) {
	// Set TX Power to value received from GroundStation
	rf95.setTxPower(tx_power);
	
	// Send message and turn on TX_LED
	// Length doesn't include null-character (string terminator), so add 1
	rf95.send(msg, strlen((char*)msg) + 1);
	digitalWrite(TX_LED, HIGH);
	
	// Wait until packet is completely sent and turn off TX_LED
	rf95.waitPacketSent();
	digitalWrite(TX_LED, LOW);
	
	// Set RFM96W back to receiver mode so that it can receive any new messages
	rf95.setModeRx();
}


// Calculate CRC16-CCITT checksum
// Taken from: http://stackoverflow.com/questions/10564491/function-to-calculate-a-crc16-checksum
uint16_t crc16(const uint8_t* data_p, uint8_t length){
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}