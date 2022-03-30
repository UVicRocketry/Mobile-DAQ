# Mobile-DAQ

UVR's Mobile Data Acquisition (DAQ) unit. This device was developed to support UVR testing programs as it supports live data display and logging to a micro SD card. The Mobile DAQ is capable of logging from load cells,
pressure transducers, and k-type thermocouples. 

## SETUP

To setup the device, first power on the device by switching the black power button on the side of the device. After a few seconds of function and systems initialization, an onboard blue LED
will turn on for 30 seconds to indicate that the device is ready to connect to. 

Scan the Network QR code with a mobile device or manually connect to the access point "UVR Mobile DAQ". Understand that there is no access to the web with this device. Once connected (no internet),
scan the Server QR code with the same mobile device or type "http://192.168.4.1/" into your browser. You should see "UVR Mobile DAQ" and the "Log/Live" tab feed.

### CONFIGURATION

In the configuration tab you can setup each sensor as well as re-mount the SD card if it failed to load on startup or if you swap the SD card during testing.

#### LOAD CELL

#### THERMOCOUPLE

#### PRESSURE TRANSDUCER

## USE

When powered on the DAQ should be left on during the entire duration of testing as connecting to the access point can take up to 60 seconds.

Connect all sensors and configure if required. When ready to log, press "Log".

## HARDWARE

1. ESP32
2. HX711
3. MAX31855
4. 18650
5. Adafruit 5v Micro SD Card
