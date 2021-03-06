#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "SSD1306.h"
#include "ESP8266TrueRandom.h"
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Servo.h>

// use arduino library manager to get libraries
// sketch->include library->manage libraries
// WiFiManager, ArduinoJson, PubSubClient, ArduinoOTA, SimpleDHT, "ESP8266 and ESP32 Oled Driver for SSD1306 display"
// wget https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip
// sketch->include library->Add .zip Library

#define TRIGGER_PIN 0
#define ACTIVATE_MAX 8
#define HALF_HEIGHT 64

const char* serverIndex = "<html><head><script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/smoothie/1.34.0/smoothie.js'></script> <script type='text/javascript' src='https://cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.js'></script> <script type='text/Javascript'> \
var sc = new SmoothieChart({ responsive: true, millisPerPixel: 10, labels: { fontSize: 30, precision: 0 }, grid: { fillStyle: '#6699ff', millisPerLine: 100000, verticalSections: 10 } }); \
var line1 = new TimeSeries(); var line2 = new TimeSeries(); var line3 = new TimeSeries(); \
sc.addTimeSeries(line1, { strokeStyle:'rgb(180, 50, 0)', fillStyle:'rgba(180, 50, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line2, { strokeStyle:'rgb(0, 255, 0)', fillStyle:'rgba(255, 0, 0, 0.4)', lineWidth:3 }); \
sc.addTimeSeries(line3, { strokeStyle:'rgb(0, 50, 255)', fillStyle:'rgba(255, 50, 0, 0.4)', lineWidth:3 }); \
$(document).ready(function() { sc.streamTo(document.getElementById('graphcanvas1')); }); \
setInterval(function() { $.getJSON('/stats',function(data){ line1.append(Date.now(), data.angle); line2.append(Date.now(), data.magnitude);}); }, 100); \
</script></head><body>  <canvas id='graphcanvas1' style='width:100%; height:75%;' /></body></html>";

int angle1 = 20;
int angle2 = 55;
bool shouldSaveConfig = false;
long lastMsg = 0;
long lastReading = 0;
long lastSwap = 0;
char msg[200];
char errorMsg[200];
int reconfigure_counter = 0;
int activate = ACTIVATE_MAX;
Servo myservo;

char name[20] = "Robot1";
char mqtt_server[20] = "mqtt.geothunk.com";
char mqtt_port[6] = "8080";
char uuid[64] = "";
char ota_password[10] = "012345678";
char *version = "1.0";
int sdelay = 4;
int pos;

int reportGap = 5;
int curve[][2] = { {0, 0}, {64, 63}, {127, 0} };

WiFiClientSecure *tcpClient;
PubSubClient *client;
ESP8266WebServer *webServer;
SSD1306 display(0x3c,5,4);

const int MPU_addr=0x68;  // I2C address of the MPU-6050
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

void mpu_init(bool initWire, int sda, int scl) {
  if(initWire) Wire.begin(sda, scl);

  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
}

void mpu_read() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  uint16_t d = abs(AcX) - abs(AcY);
  if(d < 0)
    d = 0;
  else if(AcX < 0)
    d = -d;
  snprintf(msg, sizeof(msg), "{\"angle\": %d, \"magnitude\": %d}", d, abs(AcX) + abs(AcY));
}

void mpu_display() {
  Serial.print("AcX = "); Serial.print(AcX);
  Serial.print(" | AcY = "); Serial.print(AcY);
  Serial.print(" | AcZ = "); Serial.print(AcZ);
  Serial.print(" | Tmp = "); Serial.print(Tmp/340.00+36.53);  //equation for temperature in degrees C from datasheet
  Serial.print(" | GyX = "); Serial.print(GyX);
  Serial.print(" | GyY = "); Serial.print(GyY);
  Serial.print(" | GyZ = "); Serial.println(GyZ);
}

t_httpUpdate_return update() {
  return ESPhttpUpdate.update("http://updates.geothunk.com/updates/robotz.ino.bin");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message arrived [%s]\n", topic);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(payload);
  json.printTo(Serial);

  if(json["activate"] && json["activate"] > activate) {
    Serial.printf("\nupdating activate from bus\n");
    //activate = json["activate"];
  }
}

int mqttConnect() {
  if (client->connected()) return 1;

  Serial.print("Attempting MQTT connection...");
  if (client->connect(uuid)) {
    Serial.println("connected");
    client->subscribe("+/robots");
    return 1;
  } else {
    Serial.print("failed, rc=");
    Serial.println(client->state());
    return 0;
  }
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void clearDisplay() {
  display.clear();
  display.flipScreenVertically();
}

void setup() {
  WiFiManager wifiManager;
  bool create_ota_password = true;
  byte uuidNumber[16];
  byte uuidCode[16];
  
  Serial.begin(9600);
  Serial.println("\n Starting");
  pinMode(TRIGGER_PIN, INPUT);
  pinMode(D7, OUTPUT);
  digitalWrite(D7, LOW);
  WiFi.printDiag(Serial);
  
  display.init();
  display.setContrast(255);
  clearDisplay();
  
  ESP8266TrueRandom.uuid(uuidCode);
  ESP8266TrueRandom.uuidToString(uuidCode).toCharArray(ota_password, 7);
  ota_password[6] = '\0';

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("found /config.json");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("reading /config.json");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        // manually parse name just in case it doesn't work below
        char *nameStart = buf.get() + 6;
        char *nameEnd = strchr(nameStart, '"');
        strncpy(name, nameStart, nameEnd - nameStart);
        name[nameEnd - nameStart] = '\0';

        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          if(json["n"]) strcpy(name, json["n"]);
          if(json["mqtt_server"]) strcpy(mqtt_server, json["mqtt_server"]);
          if(json["mqtt_port"]) strcpy(mqtt_port, json["mqtt_port"]);
          if(json["uuid"]) strcpy(uuid, json["uuid"]);
          if(json["ota_password"]) {
            strcpy(ota_password, json["ota_password"]);
            ota_password[6] = '\0';
            create_ota_password = false;
          }

          printf("name=%s mqtt_server=%s mqtt_port=%s uuid=%s ota_password=%s\n", name, mqtt_server, mqtt_port, uuid, ota_password);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  Serial.println("loaded config");

  WiFiManagerParameter custom_name("name", "robot name", name, 64);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_ota_password("ota_password", "OTA password (optional)", ota_password, 6);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  if(create_ota_password) {
    Serial.println("generating ota_password");
    wifiManager.addParameter(&custom_ota_password);
    saveConfigCallback();
  }

  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  if(WiFi.SSID() && WiFi.SSID() != "") {
    String status("Connecting to ");
    status.concat(WiFi.SSID());
    status.concat(" or...");
    display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 24, status);
  }
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 14, String("Connect to this wifi"));
  display.setFont(ArialMT_Plain_16);
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, String(name));
  display.display();
  
  wifiManager.autoConnect(name);
  Serial.println("stored wifi connected");
  WiFi.setAutoConnect(true);

  strcpy(name, custom_name.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  if(uuid == NULL || *uuid == 0) {
    Serial.println("generating uuid");
    ESP8266TrueRandom.uuid(uuidNumber);
    ESP8266TrueRandom.uuidToString(uuidNumber).toCharArray(uuid, 8);
    saveConfigCallback();
  }

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["n"] = name;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["uuid"] = uuid;
    json["ota_password"] = ota_password;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");
    json.printTo(configFile);
    configFile.close();
  }

  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.setFont(ArialMT_Plain_10);
  display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 5, String("Connecting to Server"));
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(DISPLAY_WIDTH, DISPLAY_HEIGHT - 10, String(WiFi.SSID()));
  display.display();

  client = new PubSubClient(*(new WiFiClient()));
  client->setServer(mqtt_server, strtoul(mqtt_port, NULL, 10));
  client->setCallback(mqttCallback);

  tcpClient = new WiFiClientSecure();

  Serial.printf("ota_password is %s\n", ota_password);

  //ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  MDNS.begin(name);
  MDNS.addService("http", "tcp", 80);
  webServer = new ESP8266WebServer(80);
  webServer->onNotFound([]() {
    webServer->send(404, "text/plain", "File not found");
  });
  webServer->on("/", HTTP_GET, [](){
    activate = ACTIVATE_MAX;
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    webServer->send(200, "text/html", serverIndex);
  });
  webServer->on("/stats", HTTP_GET, [](){
    webServer->sendHeader("Connection", "close");
    webServer->sendHeader("Access-Control-Allow-Origin", "*");
    webServer->send(200, "application/json", msg);
  });
  webServer->begin();

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  mpu_init(false, D1, D2);
}

void paint_display() {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(DISPLAY_WIDTH, 0, String(name));
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, DISPLAY_HEIGHT - 10, WiFi.localIP().toString());
  display.drawString(0, DISPLAY_HEIGHT - 20, String(WiFi.SSID()));

  display.drawLine(curve[0][0], HALF_HEIGHT - curve[0][1], curve[1][0], HALF_HEIGHT - curve[1][1]);
  display.drawLine(curve[1][0], HALF_HEIGHT - curve[1][1], curve[2][0], HALF_HEIGHT - curve[2][1]);
  
  display.display();
}

int ratio(int x, int x1, int y1, int x2, int y2) {
  float r = (x - x1) / (float)(x2 - x1);
  return ((1.0 - r) * y1) + ((r) * y2);
}

int angle_for(int pos) {
  if(pos <= curve[1][0]) {
    return ratio(pos, curve[0][0], curve[0][1], curve[1][0], curve[1][1]);
  }
  return ratio(pos, curve[1][0], curve[1][1], curve[2][0], curve[2][1]);
}

void configure_point(int &p, int divisor) {
  while( digitalRead(TRIGGER_PIN) == LOW ) {
    delay(1);
  }

  while( digitalRead(TRIGGER_PIN) == HIGH) {
    p = (1024-analogRead(A0)) / divisor;
    paint_display();
    display.display();
    delay(1);
  }
}

void configure_action() {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, String("Changing the action"));
  display.drawString(0, 25, String("Release the button now"));
  display.display();

  configure_point(curve[0][1], 16);
  configure_point(curve[1][0], 8);
  configure_point(curve[1][1], 16);
  configure_point(curve[2][1], 16);
}

int ymap(int y) {
  return map(y, 0, 63, 5, 175);
}

int flash_pressed() {
  return digitalRead(TRIGGER_PIN) == LOW;
}

void loop() {
  int index = 0;
  char value;
  char previousValue;
  char topic_name[90];

  *errorMsg = 0;
  *msg = 0;

  client->loop();

  long now = millis();

  if (flash_pressed()) {
    configure_action();
  }

  clearDisplay();

  myservo.attach(D0);
  for (pos = 0; pos < 128; pos += 1) {
    ArduinoOTA.handle();
    webServer->handleClient();
    int y = angle_for(pos);
    display.setPixel(pos, HALF_HEIGHT - y);
    display.display();
    myservo.write(ymap(y));
    delay(analogRead(A0)/16);
    if(flash_pressed()) break;
    mpu_read();
  }
  myservo.write(ymap(angle_for(0)));
  paint_display();

  if (now - lastMsg < reportGap * 1000) {
    return;
  }
  lastMsg = now;

  paint_display();
}
