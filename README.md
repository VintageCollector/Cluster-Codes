This Arduino code is for a BMW G30 cluster. 
## Hardware & upload

1. Connect Arduino board to your PC and Seeed Studio CAN-BUS Shield **V2.0** to Mega SPI. Shield MCP2515 assumed **16 MHz, 500 kbps, CS=D9**
2. In Arduino IDE open `DisplayClientV2/DisplayClientV2.ino`, select **Tools → Board → Arduino Mega or Mega 2560/Uno**, and compile/upload. 
3. In SimHub choose **Arduino → My Hardware** and connect the detected board. In **Custom Protocol** enable **Use JavaScript**, paste the complete text from `SimHubFormula.js`, and enable a short refresh (e.g. 25–50 ms) if adjustable.
4. Note: I've used a Mega so, but you can use an Uno board also similar steps.
5. Put all the zip files in BeamNG folder inside your BeamNG's mod folder

## Arduino Libraries list ##
Here are all the libraries that you need to compile the various sketches uploaded on this server. Download the libraries.zip file from the Drive link, inside the extracted libraries folder you will find a bunch of folder. These are the various libraries used in the codes, copy all of them and paste it inside your Documents>Arduino>libraries folder. https://drive.google.com/file/d/12Rs2FqrABOIPdF6imUb3-447yJGPziGY/view?usp=sharing
