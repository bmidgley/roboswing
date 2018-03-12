#include <Servo.h>
#include <Adafruit_NeoPixel.h>

#define PIN 5
#define NUMPIXELS 16

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const int max_move = 25;
float position = 0;

AccelerationReading previousAccel;
Servo myservo;

void setup() {
  Bean.setLed(128, 0, 0);
  previousAccel = Bean.getAcceleration();
  myservo.attach(3);
  pixels.begin();
  pixels.setBrightness(64);
}

void loop() {
  int current, target, brightness, pause, x, y;

  // Get the current acceleration with a conversion of 3.91×10-3 g/unit.
  AccelerationReading currentAccel = Bean.getAcceleration();
  x = map(currentAccel.xAxis, -1023, 1023, 15, 0);
  pixels.clear();
  pixels.setPixelColor(x, pixels.Color(255,255,255));
  pixels.show();

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

