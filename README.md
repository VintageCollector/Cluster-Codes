## Hardware and installation

1. Open DisplayClientV2.ino in Arduino IDE. Select your Arduino board and upload this complete sketch folder. 
2. Use the Seeed CAN\_BUS\_Shield library providing mcp2515\_can.h
3. In SimHub select this device, named Porsche Panamera 970, in Multiple USB mode. Enable Arduino Custom Protocol and Use JavaScript, then paste the entire SimHub_formula.js. SimHub adds the newline. Do not also open the same port as a Custom Serial Device.


## Relay Dashboard Lamp wiring (12V 8 channel Arduino relay)
| Relay input | Function | Arduino Mega 2560 | Arduino Uno |
|---|---|---|---|
| IN1 | High beam | D22 | D5 |
| IN2 | Right turn indicator | D23 | D6 |
| IN3 | Left turn indicator | D24 | D7 |
| IN4 | Low beam / cluster backlight (58k) | D25 | D8 |
| IN5 | Second low-beam relay | D26 | A2 |
| IN6 | Spare | — | — |
| IN7 | Coolant warning | D28 | A0 |
| IN8 | SRS warning | D29 | A1 |

## Arduino Libraries list
Here are all the libraries that you need to compile the various sketches uploaded on this server. Download the libraries.zip file from the Drive link, inside the extracted libraries folder you will find a bunch of folder. These are the various libraries used in the codes, copy all of them and paste it inside your Documents>Arduino>libraries folder.
https://drive.google.com/file/d/12Rs2FqrABOIPdF6imUb3-447yJGPziGY/view?usp=sharing
