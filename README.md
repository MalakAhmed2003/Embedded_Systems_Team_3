# Embedded Systems Course Project

The following repository contains different files of code for running a Maze Solver Robot that explores any given maze then generates the optimized path to run 
between the start and end points, then runs on this optimized path.
The Robot also has a bluetooth module that connects to the laptop and can be controlled through a custom website (which you can also filnd its python code here).
## Running the Website:
To run the Website correctly and connect it to the Arduino through the HC-10 Bluetooth Module, follow the following steps:

1- Pair with HMSOFT

2- Make sure it appears in the Bluetooth tab in the device manager

3- In the Bluetooth tab in the Device Manager--> HMSOFT--> Details tab --> Bluetooth Device Address in the drop list --> (34:03:de:34:c7:9e)

4- Run hc10_bridge.py after changing the HC-10 known addresses to the address found in the device manager

5- Run the Website.py

6- Change the serial print and read in Arduino code to be serial#, where # is based on the pins you are connecting your Hc-10 module to
