#include <Servo.h>

const int max_move = 25;

// install Madgwick by Arduino

AccelerationReading previousAccel;
Servo myservo;

void setup() {
  Bean.setLed(128, 0, 0);
  previousAccel = Bean.getAcceleration();
  myservo.attach(3);
}

void loop() {
  int current, target;

  // Get the current acceleration with a conversion of 3.91×10-3 g/unit.
  AccelerationReading currentAccel = Bean.getAcceleration();

//  target = map(max(max(Bean.getLedRed(), Bean.getLedGreen()), Bean.getLedBlue()), 0, 255, 0, 180);
  target = map(analogRead(0), 0, 1023, 0, 180);
  Bean.setLed(target, 180-target, 0);
//  current = myservo.read();

//  if(target != current) {
//    int delta = target - current;
//    if(delta > max_move) delta = max_move;
//    if(delta < -max_move) delta = -max_move;
//    current += delta;
//    myservo.write(current);
//  }

  myservo.write(target);
  previousAccel = currentAccel;                                        
  Bean.sleep(100);
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

