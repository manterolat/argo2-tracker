# Argo 2 Ground Station

This is the Ground Station program used to receive, interpret, display and upload messages from the tracker.

The program connects to the receiver through a serial port (USB) and will display messages received.
There is also the option to send received data to [HabHub](https://tracker.habhub.com). It also allows the user to send configuration messages (custom or prepared) to the tracker.

Additionally, the application checks incoming messages for errors and displays latest tracker data in a table. It also allows the user to see the last received location of the tracker through Google Maps and a geolocation-encoded QR code. 


## Table of Contents

 * [Installation](#installation)
 * [Usage](#usage)
 * [Instructions](#instructions)


<a name="installation"></a>

## Installation
To download the files in this repository run:

`git clone https://github.com/manterolat/argo2-tracker.git`

GroundStation runs on Python 2.7, and requires the following modules:
 * *crcmod* for CRC checking
 * *pyqrcode* for generating QR codes
 * *pyserial* for serial communication

To install these (using *pip*) run:

`pip install crcmod pyqrcode pyserial`


<a name="usage"></a>

## Usage
If running on macOS with Python 2 installed manually, double-clicking `GroundStation.py` should start the program.

Otherwise, open a terminal in the argo2-tracker directory and run:

```bash
cd GroundStation
python GroundStation.py
```

The program will keep a log in the form of files: `GroundStation.log` and `sentences.log`.

**Caution: Don't toggle the _Online_ checkbox until you have setup your tracker on [HabHub](https://tracker.habhub.com) and are ready to launch/test.**


<a name="instructions"></a>

## Instructions
Once the Ground Station is running you will need to connect to the receiver and configure your callsign (if sending data to HabHub).

 1. Connect your receiver through a USB port. Make sure that you have uploaded the receiver program to the microcontroller.
 
 2. Click on the *Port* dropdown list and select your receiver. On Windows it will have a name such as `COM3`. On Linux and Mac it will likely be similar to `/dev/tty.*`.

 3. *If uploading data to [HabHub](https://tracker.habhub.com):* decide on a unique callsign and set it by clicking the *Set callsign* button.

 4. Any data received will be displayed in the main output box. To view the parsed data click on the *Show Status Window* button. A new window will open with a table containing the latest data received.

 5. Besides using HabHub, you can view the current location of the tracker by either scanning the **QR code** displayed on the main screen with a camera-enabled mobile device (which should automatically open a mapping app), or by going to `Tracking->Google Maps` on the menu toolbar (to open Google Maps on a browser window).