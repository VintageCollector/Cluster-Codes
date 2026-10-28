**Audi A8 MLB Cluster Pinout:**

| Pin | Description |
| --- | --- |
| **1** | J533 diagnostic interface
| **2** | CAN shield
| **3** | CAN low
| **4** | CAN high
| **5** | GND for fuel sensors
| **6** | Sensor for Fuel Status (1)
| **7** | Sensor for Fuel Status (1)
| **8** | Dashboard buttons
| **9** | Sensor for Fuel Status (2)
| **10** | Sensor for Fuel Status (2)
| **11** | Sensor for Fuel Status (3)
| **12** | Sensor for Fuel Status (3)
| **13** | Unused
| **14** | Unused
| **15** | Unused
| **16** | Unused
| **17** | Analog clock
| **18** | GND video prep
| **19** | + Night Vision
| **20** | - Night Vision
| **21** | GND video prep
| **22** | Unused
| **23** | Clock 5V
| **24** | Unused
| **25** | Unused
| **26** | Unused
| **27** | Unused
| **28** | Unused
| **29** | Terminal 31 (GND)
| **30** | Terminal 31 (GND)
| **31** | Terminal 30 (+12V)
| **32** | Terminal 30 (+12V)

## Cluster Menu Navigation Pinout

D5 - UP
D6 - DOWN
D4 - LEFT
D7 - RIGHT
D3 - OK

## Arduino Libraries list ##
Here are all the libraries that you need to compile the various sketches uploaded on this server. Download the libraries.zip file from the Drive link, inside the extracted libraries folder you will find a bunch of folder. These are the various libraries used in the codes, copy all of them and paste it inside your Documents>Arduino>libraries folder. https://drive.google.com/file/d/12Rs2FqrABOIPdF6imUb3-447yJGPziGY/view?usp=sharing

## Youtube Clip Code (@ Courtesy of my friend Albert):
I've added an extra folder called Albert. It contains two .hex arduino files that can be flashed with X-loader app on to a Mega or Uno. Make sure to set the can bus shield CS pin to 9. 
https://www.youtube.com/watch?v=DfoEeDiIlP4
