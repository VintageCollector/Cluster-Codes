## Hardware and installation

1. Open Panamera970/Panamera970.ino in Arduino IDE. Select your Arduino board and upload this complete sketch folder. 
2. Use the Seeed CAN\_BUS\_Shield library providing mcp2515\_can.h
3. In SimHub select this device, named Porsche Panamera 970, in Multiple USB mode. Enable Arduino Custom Protocol and Use JavaScript, then paste the entire SimHubFormula\_Panamera970.js. SimHub adds the newline. Do not also open the same port as a Custom Serial Device.
4. For BeamNG SH Telemetry/Extra, close the game, replace its active mods/simhubextras.zip with BeamNG/simhubextras.zip from this package, and restart. Keep one active copy and put the backup outside mods. 

## 

## Launch-control rule

Enabled for BeamNG, ETS2, ATS and original Assetto Corsa only:

1. Speed should be 0mph/kmph
2. RPM is greater than 4000, hold Brake and press Gas Pedal at same time.
3. Gear in Drive or first Drive/Sport: D, D1, S1, A1 or M1.
4. ESC/DSC (TC) is OFF.

## LCD button wiring
Use four normally-open momentary buttons, matching the user's original test sketch:
Function	Mega digital pin	Other button terminal
Up	- D6	
Down	- D4	
OK / Enter	- D5	
Back / Return- 	D7	

## Arduino Libraries list
Here are all the libraries that you need to compile the various sketches uploaded on this server. Download the libraries.zip file from the Drive link, inside the extracted libraries folder you will find a bunch of folder. These are the various libraries used in the codes, copy all of them and paste it inside your Documents>Arduino>libraries folder.
https://drive.google.com/file/d/12Rs2FqrABOIPdF6imUb3-447yJGPziGY/view?usp=sharing
