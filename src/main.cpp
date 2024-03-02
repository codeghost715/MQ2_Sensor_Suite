#include <Arduino.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <LoRa.h>

//LORA BRANCH//

//CONSTANTS//
const char* ssid = "Farm House";
const char* password = "surelogon";
const char* GAS_ID = "AKfycbyEIXPYwkqpcjctBrpYTkOeh4Gt4ct_GiUkPCgjnf03zn1sp3EDmJIQUNByTlH0mfdV";  // GAS Deployment ID

#define MQ2_PIN 33    // Adjust for your ESP32's pin numbering for MQ2
#define MQ5_PIN1 35   // Adjust for your ESP32's pin numbering for the first MQ5
#define MQ5_PIN2 32   // Adjust for your ESP32's pin numbering for the second MQ5
#define DHTPIN 4      // what pin the DHT is connected to
#define DHTTYPE DHT22 // DHT 11 or DHT22

// LoRa pins
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2

//GLOBAL VARIABLES//
RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastTime = 0;
unsigned long interval = 60000; // Interval for sending data to Google Sheets (60 seconds)
int counter = 0; // Counter for LoRa packets

void setup() {

  ///////DO NOT MOVE THIS/////////////////////////////////////////////
  Serial.begin(9600); // Start serial communication at 9600 baud rate
  ////////////////////////////////////////////////////////////////////
  
  //Initialize WeeFee
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

  //Initialize LoRa
  //915E6 for North America
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) {  // Adjust frequency to your region ie NA
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  //Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  //Initialize DHT22
  dht.begin();

 // Initialize sensor pins
  pinMode(MQ2_PIN, INPUT); // Set MQ2 pin as an input
  pinMode(MQ5_PIN1, INPUT); // Set first MQ5 pin as an input
  pinMode(MQ5_PIN2, INPUT); // Set second MQ5 pin as an input
} //THIS CLOSES THE VOID SETUP

void loop() {

  // Read sensors
  DateTime now = rtc.now();
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  float temperatureF = dht.readTemperature(true);
  int mq2Value = analogRead(MQ2_PIN);
  int mq5Value1 = analogRead(MQ5_PIN1);
  int mq5Value2 = analogRead(MQ5_PIN2);

  // Check sensor readings validity
  if (!isnan(humidity) && !isnan(temperatureC)) {
    // Print sensor readings to serial
    Serial.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.print(" | Humidity: ");
    Serial.print(humidity);
    Serial.print("% | Temp: ");
    Serial.print(temperatureC);
    Serial.print("°C ");
    Serial.print(temperatureF);
    Serial.print("°F | MQ2 Value: ");
    Serial.print(mq2Value);
    Serial.print("ppm | MQ5 Value 1: ");
    Serial.print(mq5Value1);
    Serial.print("ppm | MQ5 Value 2: ");
    Serial.println(mq5Value2);
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }

  // Send data over LoRa
    LoRa.beginPacket();
    LoRa.print("Counter: ");
    LoRa.print(counter);
    LoRa.print("Hello, LoRa!");
    LoRa.endPacket();
    counter++;

  // Send data to Google Sheets every minute
  if (millis() - lastTime > interval) {
    lastTime = millis();

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(String("https://script.google.com/macros/s/") + String(GAS_ID) + String("/exec")); // Google Script URL
      http.addHeader("Content-Type", "application/json");

      String payload = "{\"temperature\":" + String(temperatureC) +
                       ",\"humidity\":" + String(humidity) +
                       ",\"mq2Value\":" + String(mq2Value) +
                       ",\"mq5Value1\":" + String(mq5Value1) +
                       ",\"mq5Value2\":" + String(mq5Value2) + "}";

      int httpResponseCode = http.POST(payload);

      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
      }

      http.end(); // Free resources
    }
  }

  delay(1000); // Delay for 1 second for the serial print
}