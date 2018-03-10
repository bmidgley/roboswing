# roboswing
Arduino sketches to experiment with robots that swing themselves back and forth

* periodic-esp8266 contains a sketch for d-duino to create a curve and control its frequency from a potentiometer
* periodic-bean contains a sketch for lightblue bean to control the robot's frequency from the Lightblue app
* driven-bean contains a sketch to drive the lightblue bean's servo directly from a potentiometer

For each sketch, connect an sg90 with data going to lightblue bean pin 3 and d-duino pin D0. I use a USB power
supply and connect the sg90 to +5v to get a more stable motion.
