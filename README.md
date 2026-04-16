# Embedded Systems Course Project

The following repository contains different files of code for running a Maze Solver Robot that explores any given maze then generates the optimized path to run 
between the start and end points, then runs on this optimized path.
The Robot also has a bluetooth module that connects to the laptop and can be controlled through a custom website (which you can also filnd its python code here).
## Exploration and Optimized Path:
To get the Robot to explore and optimize the path taken to solve the maze:
1- Upload the Maze_Solver.ino code to the Robot Arduino 
2- Power up the system with ~ 9.4-9.6 V

### Code Architecture
<img width="1459" height="1143" alt="maze_robot_architecture" src="https://github.com/user-attachments/assets/ffb2bc7a-26dc-47a9-a5d7-ae5c109fe859" />

## Running the Website:
To run the Website correctly and connect it to the Arduino through the HC-10 Bluetooth Module, follow the following steps:

1- Pair with HMSOFT.

2- Make sure it appears in the Bluetooth tab in the device manager.

3- In the Bluetooth tab in the Device Manager--> HMSOFT--> Details tab --> Bluetooth Device Address in the drop list --> (34:03:de:34:c7:9e)

4- Run hc10_bridge.py after changing the HC-10 known addresses to the address found in the device manager.

5- Run the Website.py

6- Change the serial print and read in Arduino code to be serial#, where # is based on the pins you are connecting your Hc-10 module to.

The Website should look like:
<img width="1375" height="1580" alt="image" src="https://github.com/user-attachments/assets/9bee1f54-0cdc-491d-9f62-e2a36fb4140e" />




