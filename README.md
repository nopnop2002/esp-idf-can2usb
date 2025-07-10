# esp-idf-can2usb
CANbus to USB bridge using esp32.   
The ESP32-S2/S3 has a full-speed USB OTG peripheral with integrated transceivers and is compliant with the USB 1.1 specification.   
GPIO19 and GPIO20 can be used as D- and D + of USB respectively.   

It's purpose is to be a bridge between a CAN-Bus and a USB OTG.    
Unlike the standard serial port, the USB OTG port does not display initial output from the ROM bootloader.   

![slide0001](https://user-images.githubusercontent.com/6020549/124847532-0718e700-dfd6-11eb-99f8-45ffef024304.jpg)

# Software requirement
esp-idf v4.4/v5.0.   
This is because this version supports ESP32-S3.   

# Hardware requirements
1. ESP32-S2/S3 Development board   
Because the ESP32-S2/S3 does support USB OTG.   

2. SN65HVD23x CAN-BUS Transceiver   
SN65HVD23x series has 230/231/232.   
They differ in standby/sleep mode functionality.   
Other features are the same.   

3. Termination resistance   
I used 150 ohms.   

4. USB Connector   
I used this USB Mini femail:   
![usb-connector](https://user-images.githubusercontent.com/6020549/124848149-3714ba00-dfd7-11eb-8344-8b120790c5c5.JPG)

```
ESP32-S2/S3 BOARD          USB CONNECTOR
                           +--+
                           | || VCC
    [GPIO 19]    --------> | || D-
    [GPIO 20]    --------> | || D+
    [  GND  ]    --------> | || GND
                           +--+
```

This connector is used with USB OTG.   

# Wireing   
|SN65HVD23x||ESP32-S2/S3||
|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO17|(*1)|
|GND|--|GND||
|Vcc|--|3.3V||
|R(CRX)|--|GPIO18|(*1)|
|Vref|--|N/C||
|CANL|--||To CAN Bus|
|CANH|--||To CAN Bus|
|RS|--|GND|(*2)|

(*1) You can change using menuconfig.

(*2) N/C for SN65HVD232

# Test Circuit   
```
   +-----------+ +-----------+ +-----------+ 
   | Atmega328 | | Atmega328 | | ESP32-S2  | 
   |           | |           | |           | 
   | Transmit  | | Receive   | | 17    18  | 
   +-----------+ +-----------+ +-----------+ 
     |       |    |        |     |       |   
   +-----------+ +-----------+   |       |   
   |           | |           |   |       |   
   |  MCP2515  | |  MCP2515  |   |       |   
   |           | |           |   |       |   
   +-----------+ +-----------+   |       |   
     |      |      |      |      |       |   
   +-----------+ +-----------+ +-----------+ 
   |           | |           | | D       R | 
   |  MCP2551  | |  MCP2551  | |   VP230   | 
   | H      L  | | H      L  | | H       L | 
   +-----------+ +-----------+ +-----------+ 
     |       |     |       |     |       |   
     +--^^^--+     |       |     +--^^^--+
     |   R1  |     |       |     |   R2  |   
 |---+-------|-----+-------|-----+-------|---| BackBorn H
             |             |             |
             |             |             |
             |             |             |
 |-----------+-------------+-------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

__NOTE__   
3V CAN Trasnceviers like VP230 are fully interoperable with 5V CAN trasnceviers like MCP2551.   
Check [here](http://www.ti.com/lit/an/slla337/slla337.pdf).


# Installation for ESP32-S2/S3
```
git clone https://github.com/nopnop2002/esp-idf-can2usb
cd esp-idf-can2usb
idf.py set-target {esp32s2/esp32s3}
idf.py menuconfig
idf.py flash
```

# Configuration
![config-main](https://user-images.githubusercontent.com/6020549/124848270-7e02af80-dfd7-11eb-931c-ebb1653a276f.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/124848275-7f33dc80-dfd7-11eb-8a15-1ea217417e34.jpg)

# Definition from CANbus to USB
When CANbus data is received, it is sent by USB OTG according to csv/can2usb.csv.   
The file can2usb.csv has three columns.   
In the first column you need to specify the CAN Frame type.   
The CAN frame type is either S(Standard frame) or E(Extended frame).   
In the second column you have to specify the CAN-ID as a hexdecimal number.   
In the last column you have to specify the Frame Name. This project does not use this column.   

```
S,101,Water Temperature
E,101,Water Pressure
S,103,Gas Temperature
E,103,Gas Pressure
```

# Brows data Using Windows Terminal Software
When you connect the USB cable to the USB port on your Windows machine and build the firmware, a new COM port will appear.   
Open a new COM port in the terminal software.   
I used TeraTerm.   
![teraterm](https://user-images.githubusercontent.com/6020549/124849184-43017b80-dfd9-11eb-9c28-ce63b98395bf.jpg)

# Brows data Using Linux Terminal Software
When you connect the USB cable to the USB port on your Linux machine and build the firmware, a new /dev/tty device will appear.   
Open a new tty device in the terminal software.   
Most occasions, the device is /dev/ttyACM0.   
I used screen.   
![screen](https://user-images.githubusercontent.com/6020549/124849312-79d79180-dfd9-11eb-9e58-044af2166632.jpg)

# Brows data Using python script
You can use read.py script. ```python read.py```   
![python](https://user-images.githubusercontent.com/6020549/124849418-b3100180-dfd9-11eb-869e-21b47505354a.jpg)


# Powerd from USB OTG   
After writing the firmware, the ESP32 can get power from the USB OTG.   
```
ESP32-S2/S3 BOARD          USB CONNECTOR
                           +--+
    [  VIN  ]    --------> | || VCC
    [GPIO 19]    --------> | || D-
    [GPIO 20]    --------> | || D+
    [  GND  ]    --------> | || GND
                           +--+
```

# Troubleshooting   
There is a module of SN65HVD230 like this.   
![SN65HVD230-1](https://user-images.githubusercontent.com/6020549/80897499-4d204e00-8d34-11ea-80c9-3dc41b1addab.JPG)

There is a __120 ohms__ terminating resistor on the left side.   
![SN65HVD230-22](https://user-images.githubusercontent.com/6020549/89281044-74185400-d684-11ea-9f55-830e0e9e6424.JPG)

I have removed the terminating resistor.   
And I used a external resistance of __150 ohms__.   
A transmission fail is fixed.   
![SN65HVD230-33](https://user-images.githubusercontent.com/6020549/89280710-f7857580-d683-11ea-9b36-12e36910e7d9.JPG)

If the transmission fails, these are the possible causes.   
- There is no receiving app on CanBus.
- The speed does not match the receiver.
- There is no terminating resistor on the CanBus.
- There are three terminating resistors on the CanBus.
- The resistance value of the terminating resistor is incorrect.
- Stub length in CAN bus is too long. See [here](https://e2e.ti.com/support/interface-group/interface/f/interface-forum/378932/iso1050-can-bus-stub-length).

# Reference

https://github.com/nopnop2002/esp-idf-candump

https://github.com/nopnop2002/esp-idf-can2mqtt

https://github.com/nopnop2002/esp-idf-can2http

https://github.com/nopnop2002/esp-idf-can2socket

https://github.com/nopnop2002/esp-idf-can2websocket

https://github.com/nopnop2002/esp-idf-CANBus-Monitor
