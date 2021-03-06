# Argo 2 Tracker

High Altitude Balloon tracker designed for the Argo 2 launch by the Near Space Program at the International School of Panama.

Tracker transmits temperature, pressure and humidity data, as well as location coordinates and other status information.

The GroundStation program shows any data received, along with signal strength and CRC checking. It can also upload new data to the online tracker at [HabHub](https://tracker.habhub.com).

Included are the PCB schematics (made in Eagle CAD) and software of the tracker and receiver, as well as the Ground Station program which runs on the tracking computer.

The software for the tracker and receiver is written in Arduino / C++, while the Ground Station written in Python.


## Table of Contents
 * [Parts and Components](#parts)
 * [Installation](#installation)
 * [Usage](#usage)
 * [Testing](#testing)
 * [Printed Circuit Boards](#pcbs)
 * [Launches](#launch)

<a name="parts"></a>

## Parts and Components
### Tracker:
 * [Adafruit Feather M0](https://www.adafruit.com/product/269), or [Teensy 3.x](https://www.pjrc.com/store/teensy32.html) with [Feather adapter](https://www.adafruit.com/products/3200)
 * U-Blox NEO 6/7/8 Serial GPS module such as [this](https://www.amazon.com/Antenna-AeroQuad-Multirotor-Quadcopter-Aircraft/dp/B00RCP9MLY/)
 * ~~[MAX31855 thermocouple amplifier](https://www.adafruit.com/products/269) (and [thermocouple](https://www.adafruit.com/products/270))~~ Doesn't work
 * [BME280 module](https://www.adafruit.com/products/2652): Pressure, humidity and temperature sensor
 * RFM95W Transceiver

### Receiver:
 * [Adafruit Feather M0](https://www.adafruit.com/product/269)
 * RFM95W Transceiver
 * PCB SMA connector (if using SMA cable for antenna)


<a name="installation"></a>

## Installation
To download the files in this repository run:

`git clone https://github.com/manterolat/argo2-tracker.git`

### Tracker:
#### Software
 * [Arduino IDE](https://www.arduino.cc/en/Main/Software) (1.6.0 and above should work)
 * If using *Teensy 3.x*: Install [Teensyduino](http://www.pjrc.com/teensy/teensyduino.html) (tested with Teensyduino 1.29)
 * If using *Feather M0*: follow [these](https://learn.adafruit.com/adafruit-feather-m0-basic-proto/setup) instructions

#### Arduino Libraries
 * [Adafruit Unified Sensor Driver](https://github.com/adafruit/Adafruit_Sensor)
 * [Adafruit BME280](https://github.com/adafruit/Adafruit_BME280_Library)
 * ~~Only with *Teensy* setup: [Adafruit MAX31855](https://github.com/adafruit/Adafruit-MAX31855-library) (version 1.0.3 or higher for SPI Transactions)~~ Doesn't work
 * If using *Teensy 3.x*: [RadioHead](https://github.com/PaulStoffregen/RadioHead) (Paul Stoffregen's version for Teensy)
 * If using *Feather M0*: [RadioHead](http://www.airspayce.com/mikem/arduino/RadioHead/) (Regular, updated version) 
 * ~~[TinyGPS++](https://github.com/mikalhart/TinyGPSPlus)~~ Broken for non-AVR chips: use [this](https://github.com/manterolat/argo2-tracker/files/601752/TinyGPSPlus.zip) modified version instead

### Receiver:
#### Software
 * [Arduino IDE](https://www.arduino.cc/en/Main/Software) (1.6.0 and above should work)

#### Arduino Libraries
 * [RadioHead](https://github.com/PaulStoffregen/RadioHead)

### Ground Station:
GroundStation runs on Python 2.7, and requires the following modules:
 * *crcmod* for CRC checking
 * *pyqrcode* for generating QR codes
 * *pyserial* for serial communication

To install these (using *pip*) run:

`pip install crcmod pyqrcode pyserial`



<a name="usage"></a>

## Usage
After setting up the software and libary requirements you should be able to upload the tracker and receiver programs, as well as run the Ground Station.

### Tracker:
Open `/src/Argo2_Tracker/Argo2_Tracker.ino` in the Arduino IDE.

Before compiling and uploading to the Feather/Teensy make sure to configure the tracker by changing the following lines in the program: 
```C++
// Tracker callsign: change to your tracker's callsign
#define CALLSIGN "CHANGE_ME"

// Debug messages: uncomment the following line to enable printing to serial (for testing purposes)
//#define DEBUG

...

// Select either Teensy 3.x or Feather M0
#define TEENSY
//#define FEATHER
```

Leaving `#define DEBUG` uncommented will compile the Serial messages, which takes up space and cycles in the microcontroller.

Finally, select the board type and serial port in the Arduino IDE and upload.  

### Receiver:
Open `Argo2_Receiver.ino` with the Arduino IDE, then select the Feather M0 from the boards list and upload.


### Ground Station:
If running on macOS with Python 2 installed manually, double-clicking `GroundStation.py` should start the program.

Otherwise, open a terminal in the argo2-tracker directory and run:

```bash
cd GroundStation
python GroundStation.py
```

The program will keep a log in the form of files: `GroundStation.log` and `sentences.log`.

**Caution: Don't toggle the _Online_ checkbox until you have setup your tracker on [HabHub](https://tracker.habhub.com) and are ready to launch/test.**


<a name="testing"></a>

## Testing
Things to keep in mind before launch:
 * *Cut the thermocouple cable as short as possible* while keeping a small piece outside. We found that this reduced interference and/or resistance in the cable and improved the accuracy of measurements.
 * For the BME280 (pressure and humidity sensor) it's best to use cables and *place the module on the outside of the capsule*.
 * If placing the BME280 outside, *cover the sensor with a small piece of cotton/fabric using tape* in order to block wind which could affect measurements.

Only the *Adafruit Feather M0* configuration (without thermocouple sensor) has being tested so far. Since then there have been a few changes to the tracker and receiver programs (mostly changing Serial.println to Snprintln).

The tracker also can't use the BME280 through SPI at the same time as the RFM95W radio due to SPI Transaction issues with the RadioHead library.
For this reason, **to use the BME280 you will need to connect the SCL and SDA pins of the BME280 to those of the Feather externally to use I2C instead** (soldering wires externally).


<a name="pcbs"></a>

## Printed Circuit Boards
Both of these boards can be used with headers (for removable/replaceable components), or with the components soldered directly unto the board. I recommend the first method.

If using headers note that the RFM95W module needs *2.0mm* headers, instead of regular 2.54mm headers.

The PCBs used in the Argo 2 launch were printed by [OSH Park](https://oshpark.com/) and functioned perfectly.

### Tracker:
Note that there is an issue with the buzzer MOSFET and its connections at the moment. If you plan to use this board with a buzzer, you will have to either fix the MOSFET connections or connect the components separately.

<img src="hardware/tracker/tracker_pcb.png" width="450"> 

### Receiver:
This board supports the use of a PCB to SMA adapter if needed.

<img src="hardware/receiver/receiver_pcb.png" width="300">


<a name="launch"></a>

## Launches
### **April 23, 2016**: (Argo-2) 
We launched the Argo 2 capsule on April 23, 2016 using the Feather M0 setup without the thermocouple (due to SPI issues). We received data throughout most of flight, which reached `33 km` altitude, and used HabHub to follow the capsule.
Unfortunately, the capsule landed in a mountainous area that made recovery impossible.

The data revealed an average vertical speed of `2.3 m/s` (very low) and an estimated flight time of 295 minutes.

### **November 28, 2016**: (CP-1)

<img src="img/G0022041.jpg" width="400">
<img src="img/G0036804.jpg" width="400">

We launched the CP-1 capsule on November 28, 2016, and succesfully recovered it! GoPro camera took many photos throughout the mission.

The capsule rose to around `33 km` at an average vertical speed of `4.5 m/s`. The mission lasted 149 minutes.

### **June 4, 2017**: (CP-2)
Another successfull recovery with the same setup!

The capsule rose to around `27 km` at an average vertical speed of `5.5 m/s`. The mission lasted 136 minutes.

In the future, the NSP team will be using the [Pi in the Sky](http://www.pi-in-the-sky.com/) board due to its widespread support and easy configurability. I wish them the best of luck in future launches!