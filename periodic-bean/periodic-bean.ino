#include <Servo.h>

const int max_move = 25;
float position = 0;

AccelerationReading previousAccel;
Servo myservo;

void setup() {
  Bean.setLed(128, 0, 0);
  previousAccel = Bean.getAcceleration();
  myservo.attach(3);
}

void loop() {
  int current, target, brightness, pause;

  // Get the current acceleration with a conversion of 3.91×10-3 g/unit.
  AccelerationReading currentAccel = Bean.getAcceleration();

  // swing servo
  target = map(1000 * sin(position), -1000, 1000, 30, 150);
  myservo.write(target);
  position += 1.0;

  // pause according to remote control
  brightness = max(max(Bean.getLedRed(), Bean.getLedGreen()), Bean.getLedBlue());
  pause = map(brightness, 0, 255, 0, 1000);
  
  previousAccel = currentAccel;                                        
  Bean.sleep(pause);
}

int getMagnitude(AccelerationReading reading){
  int x = reading.xAxis * reading.xAxis;
  int y = reading.yAxis * reading.yAxis;
  int z = reading.zAxis * reading.zAxis;
  return sqrt(x + y + z);   
}

int getAccelDifference(AccelerationReading readingOne, AccelerationReading readingTwo){
  int deltaX = abs(readingTwo.xAxis - readingOne.xAxis);
  int deltaY = abs(readingTwo.yAxis - readingOne.yAxis);
  int deltaZ = abs(readingTwo.zAxis - readingOne.zAxis);
  // Return the magnitude
  return deltaX + deltaY + deltaZ;   
}

