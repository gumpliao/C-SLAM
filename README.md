C-Slam
======

[C-Slam](http://airlab.ws.dei.polimi.it/index.php/C-SLAM), Is a Cognitive Self Localization And Mapping fremework, that works using knowledge about the world to recognise objects,, create a map and localize the robot.
The aim of this project is to work get a realiable SLAM system in noisy and mutable envirorment to perform navigation and path plannning tasks with low computational power, not to get a high precision SLAM system.


USAGE
-----

You need to install the [ROS middleware](www.ros.org) to compile and execute the code in this repository. Furthermore, some part of the code need the OpenCV libraries to be installed on your system (this should be automatically done by ROS), `flex` and `bison`.


COMPILING
---------

The system can be build using the ros build tool `catkin`. Just create a catkin workspace, put the content of this repository in the src repository and run `catkin_make` to build the system.
check [this](http://ros.org/wiki/catkin/Tutorials/create_a_workspace) tutorial to get more info on catkin.


Copyright & contacts
--------------------

This project is distributed under the GNU GPL license, version 3.

(C) 2012 Davide Tateo

(C) 2012 Politecnico Di Milano

For further information, please contact davide.tateo90@gmail.com