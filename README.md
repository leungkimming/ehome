# eHome
*Monitor Home electricity consumption using SCT-013, Emonlib, ESP32 & LED display*
### Author: Michael Leung from Hong Kong

## Intro
![eHome](/doc/IMG_5466.jpg)

This device provides the following functions:
* Monitor home electricity consumption per second with realtime display in Ampere
* With 10% change for 10 seconds, record to SD card
* Trigger alarm when over predetermine Ampere
* HTTP get current consumption
* HTTP get history
* HTTP restful API to adjust parameters

## References
* https://learn.openenergymonitor.org/electricity-monitoring/ct-sensors/interface-with-arduino
* https://github.com/openenergymonitor/EmonLib
* https://github.com/avishorp/TM1637.git
* https://github.com/me-no-dev/ESPAsyncWebServer.git
* https://community.openenergymonitor.org/t/sampling-rate-of-emonlib/4383

## Components
* Current Transformer: https://www.ebay.com/itm/174480353810
* ESP32: https://www.amazon.com/DORHEA-Bluetooth-Internet-Development-Functional/dp/B08ND91YB8?th=1
* SD card module: https://www.ebay.com/itm/293683210627
* LED display: https://www.amazon.com/diymore-Display-7-Segment-4-Digit-Digital/dp/B078J2BRRB
* 3.5mm socket x 2
* buzzer
* 10k resistor x 2
* 10uf capacitor
* 0.1uf capacitor
* Diode IN4004
* plastic box: https://www.ebay.com/itm/383751696130 
* double sided circult board: https://www.amazon.com/GFORTUN-Protoboard-Prototyping-Soldering-Universal/dp/B06XR1JWNY
* USB charging cable
* 3.5mm plug

## Schematic circuit diagram
![Schematic](/doc/ehome.png)

## Circuit Board design
![Schematic](/doc/ehome_circuit.png)

## Assembly
![Assembly](/doc/IMG_5459.jpg)
Solder the bottom side, with ESP32 socket, buzzer, 5v 3.5mm plug, diode and wiring.
As seen in the picture, you need to cut one side of the circuit board to fit into the box. The 4 pin socket on the side you have cut is just a dummy to make the circuit board sitting flat.<br />
Prepare LED display on the cover top. Don't mount and drill holes for the 4 pins connector yet. Needs precise alignment with the top side socket.

![Assembly](/doc/IMG_5460.jpg)
Solder the top side, with resistors, capacitors, 3.5mm socket for SCT-13-060, 4 pins sockets for LED display and 6 pins socket for SD card module. I used the 0.1uf capacitor to mount the 3.5mm socket on the board because I want to cancel noise from the outset.<br />
Mount the SD card module on the cover bottom. Need careful alignment with the 6 pins socket. Mount the LED display on cover top with holes for the 4 pins. Again, need careful aligment with the 4 pins socket.

![Assembly](/doc/IMG_5462.jpg)
Plug in ESP32, and the cover assembly with both LED display and SD module onto the respective sockets.

![Assembly](/doc/IMG_5463.jpg)
Drill holes on each side of the plastic box for the two 3.5mm sockets. Again, you need very careful alignment. <br />
Close the plastic box and fasten the 3.5mm socket nuts.

![Assembly](/doc/IMG_5471.JPG)
**Warning!** Employ a licensed eletrical technician to plug in the SCT-13-060 to your home's main supply.

![eHome](/doc/IMG_5466.jpg)
Cut one end of the USB cable and replace it with the 3.5mm plug. I used a wall plug with USB charging to supply the +5v power supply.

## HTTP functions
You have to hard code 
* your wifi routers SSID and Password
* your region's NTP server URL

### 1. Current consumption http://xxx.xxx.x.xxx/
```
2021/10/31 00:05:52 - 1.55A
```

### 2. History http://xxx.xxx.x.xxx/history
```
Date_Time,Amp
2021/10/28 14:17:14,2.76
2021/10/28 14:21:34,0.63
2021/10/28 19:51:35,7.74
```
### 3. Clear all History records http://xxx.xxx.x.xxx/clear

### 4. Set alert level http://xxx.xxx.x.xxx/alert?value=3300
3300 with assumed 2 decimal point = 33.00A

### 5. CT Calibration http://xxx.xxx.x.xxx/ctadj?value=60
We need a "Current Constant" to create the emonlib object.
```
emon1.current(ESP32 ADC pin, Current Constant);
```
Based on the calibration theory on learn.openenergymonitor.org, If you use a current transformer with a built-in burden (SCT-013-060 is one of these), then
```
current constant = max primary current ÷ (max secondary current (I) * burden resistor (R))
current constant = max primary current ÷ (max voltage across the burden resistor)
```
For SCT-013-060, we know "max primary current" = 60 and "max voltage across the burden resistor" = 1V. Hence, 
```
current constant = 60 ÷ (1) = 60
emon1.current(ESP32 ADC pin, 60);
```
### 6. Sampling cycle Calibration http://xxx.xxx.x.xxx/cycle?value=5394
Our electricity supply is in form of Alternating Current (AC), which is the number of cycles per second of sine waves in Hertz (Hz). Best sampling rates are multiple of complete sin waves. The sampling-rate-of-emonlib reference above found in his setup that:
```
calcIrms( ) will have approx 5588 current samples per second
1 cycle of mains:
112 for a 50 Hz system, or 93 for a 60 Hz system.

The smallest ‘universal’ number:
559 (100 ms, or 5 cycles of 50 Hz or 6 cycles of 60 Hz).

The recommended monitoring period:
1676 (300 ms).
```
However, in my setup, cycles to last for 300 ms is 5394.
```
  Serial.print("Cycle=");
  start = millis();
  Irms = emon1.calcIrms(5394);
  Serial.println(millis() - start);

output:
Cycle=300
```





