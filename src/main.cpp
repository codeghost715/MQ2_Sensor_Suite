#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h> //RYLR998 
#include <PMS.h>        
#include <RTClib.h>
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <LoRa.h>           //RYLR998

//ENS160 libraries
#include <ScioSense_ENS160.h>

// MQ Sensor Pins
#define MQ2_PIN 35    //MQ2 in Pin D35
#define MQ5_PIN 32    //MQ5 in Pin D32
#define MQ135_PIN 33  //MQ135 in Pin D33

// LoRa Pins
#define LORA_RX_PIN 4  // RYLR 998 LoRa TXD -> RX0 
#define LORA_TX_PIN 2  // RYLR 998 LoRa RXD -> TX0 
HardwareSerial LoraSerial(1);

// PMS5003 Pins
#define PMS_RX_PIN 16  // RX from PMS (connect to TX2 of ESP32)
#define PMS_TX_PIN 17  // TX from PMS (connect to RX2 of ESP32)
HardwareSerial PmsSerial(2); // Use UART2 for PMS5003
PMS pms(PmsSerial);
PMS::DATA data;

// ENS160 and AHT21
Adafruit_AHTX0 aht;
ScioSense_ENS160 ens;

// RTC Pins
#define RTC_PIN 22    // RTC DS3231 SCL in D22
#define RTC_PIN_SDA 21    // RTC DS3231 SDA in D21
RTC_DS3231 rtc;

// Keys and constants
const char* ssid = "Farmhouse";
const char* password = "surelogon";
const char* GAS_ID = "Your-GAS-ID";

// Interval
unsigned long lastTime = 0;
const unsigned long interval = 60000; // 60 seconds

void sendATCommand(const String& command) {
  LoraSerial.println(command);
}

void sendLoraData(String data, int address) {
  const int maxDataLength = 256;
  if (data.length() > maxDataLength) {
    Serial.println("Data too long to send");
    return;
  }

  String myString = "AT+SEND=" + String(address) + "," + String(data.length()) + "," + data + "\r\n";
  char buf[maxDataLength + 50];
  myString.toCharArray(buf, sizeof(buf));
  LoraSerial.write(buf);
  Serial.println("Message sent: " + myString);
}

void setup() {
  Serial.begin(9600);
  LoraSerial.begin(115200, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN); // LoRa Serial
  PmsSerial.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN); // PMS5003 Serial


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (!aht.begin()) {
    Serial.println("Could not find AHT sensor!");
    while (1);
  }

  if (!ens.begin()) {
    Serial.println("Could not find ENS160 sensor!");
    while (1);
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // MQ Sensors setup
  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ5_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  // LoRa initial commands
  sendATCommand("AT+ADDRESS=1");
  delay(100);
}

void loop() {
  // Read ENS160 and AHT21
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  if (ens.measure()) { // Use measure to update sensor data
    uint16_t tvoc = ens.getTVOC();
    uint16_t eco2 = ens.geteCO2();

    // Print TVOC and eCO2 values
    Serial.print("TVOC: "); Serial.println(tvoc);
    Serial.print("eCO2: "); Serial.println(eco2);
  }

  int mq2Value = analogRead(MQ2_PIN);
  int mq5Value = analogRead(MQ5_PIN);
  int mq135Value = analogRead(MQ135_PIN);

  if (PmsSerial.available()) {
    if (pms.read(data)) {
      // PM data
      Serial.print("PM 1.0: "); Serial.println(data.PM_AE_UG_1_0);
      Serial.print("PM 2.5: "); Serial.println(data.PM_AE_UG_2_5);
      Serial.print("PM 10: "); Serial.println(data.PM_AE_UG_10_0);
    }
  }

  if (millis() - lastTime > interval) {
    lastTime = millis();

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(String("https://script.google.com/macros/s/") + String(GAS_ID) + String("/exec"));
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      http.addHeader("Content-Type", "application/json");

      String payload = "{\"temperature\":" + String(temp.temperature) + ",\"humidity\":" + String(humidity.relative_humidity) + "}";
      Serial.println("Payload: " + payload);
      int httpResponseCode = http.POST(payload);

      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    }

    // Send sensor data over LoRa
    String sensorData = String(temp.temperature) + "," + String(humidity.relative_humidity) + "," + String(mq2Value) + "," + String(mq5Value) + "," + String(mq135Value);
    sendLoraData(sensorData, 2);
  }

  delay(1000);
}