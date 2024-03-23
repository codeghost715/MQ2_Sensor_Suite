#include <Arduino.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <DHT.h> //DHT 22
#include <HTTPClient.h>
#include <HardwareSerial.h> //RYLR 998 LORA MODULE

//KEYS AND CONSTANTS//
const char* ssid = "Lord Bowies Lair";
const char* password = "zaq1@wsX";
const char* GAS_ID = "AKfycbycLUnfOufySjo_6yUF0l5QokU6HDAeQ_PNhhOIge9tH66QvMN9Ik5V6BzuimgdWw4N";  // GAS Deployment ID

#define MQ2_PIN 32    // MQ2 A0 + 1kΩ -> PIN 32 
#define MQ5_PIN 33   // MQ5 A0 + 1kΩ -> PIN33
#define MQ135_PIN 35   // MQ135 A0 + 1kΩ -> PIN35
#define DHTPIN 15      // DHT -> PIN15
#define DHTTYPE DHT22 // DHT22
#define RTC_PIN 22    // RTC DS3231 module
#define LORA_RX_PIN 27   // RYLR 998 LoRa TXD -> PIN4
#define LORA_TX_PIN 4  // RYLR 998 LoRa RXD -> PIN27

//Global Variables
RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial LoraSerial(1); // Using the second hardware serial on ESP32

void sendATCommand(const String& command) {
    LoraSerial.println(command); // Send the AT command to the LoRa module
}

void setup() {
  ///////DO NOT MOVE THIS/////////////////////////////////////////////
  Serial.begin(9600); // Start serial communication at 9600 baud rate
  ////////////////////////////////////////////////////////////////////

  LoraSerial.begin(115200, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN); // RX, TX
  delay(1000); // Wait for a second after opening the new serial port
  sendATCommand("AT"); // Check if LoRa module is responsive
  delay(1000); // Wait a bit for the module to respond
  sendATCommand("AT+BAND=915000000"); // Set frequency to 915MHz
  delay(100); // Wait for the module to respond

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
  }
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  dht.begin();

  pinMode(MQ2_PIN, INPUT); // Set MQ2 pin as an input
  pinMode(MQ5_PIN, INPUT); // Set first MQ5 pin as an input
  pinMode(MQ135_PIN, INPUT); // Set second MQ5 pin as an input

//   //Initialize LoRa module with AT commands
  LoraSerial.println("AT+ADDRESS=1"); // Example: Set device address
  delay(100); // Short delay to allow for module response
//   //Add additional AT commands as needed

}

unsigned long lastTime = 0;
unsigned long interval = 60000; // interval at which to send data (60 seconds)

void sendLoRaMessage(float temperatureC, float humidity, int mq2Value, int mq5Value, int mq135Value) {
    String loraMessage = "AT+SEND=0,10,"; // Adjust according to your needs
    loraMessage += String(temperatureC) + "," + String(humidity) + "," + String(mq2Value) + "," + String(mq5Value) + "," + String(mq135Value); 
    LoraSerial.println(loraMessage);
}

void loop() {
    DateTime now = rtc.now();

    float humidity = dht.readHumidity();
    float temperatureC = dht.readTemperature();
    float temperatureF = dht.readTemperature(true);
    int mq2Value = analogRead(MQ2_PIN);
    int mq5Value = analogRead(MQ5_PIN);
    int mq135Value = analogRead(MQ135_PIN);

  if (LoraSerial.available()) {
        String response = LoraSerial.readStringUntil('\n');
        Serial.println("Response from LoRa: " + response);
    }

    if (isnan(humidity) || isnan(temperatureC)) {
        Serial.println("Failed to read from DHT sensor!");
    } 

    else {
        // Serial prints every loop iteration (every second)
        Serial.print(now.timestamp(DateTime::TIMESTAMP_FULL));
        Serial.print(" | Humidity: ");
        Serial.print(humidity);
        Serial.print("% | Temp: ");
        Serial.print(temperatureC);
        Serial.print("°C ");
        Serial.print(temperatureF);
        Serial.print("°F | MQ2 Value: ");
        Serial.print(mq2Value);
        Serial.print("ppm | MQ5 Value: ");
        Serial.print(mq5Value);
        Serial.print("ppm | MQ135 Value: ");
        Serial.print(mq135Value);
        Serial.println("ppm");
    }

    // Send data to Google Sheets every minute
    if (millis() - lastTime > interval) {
        lastTime = millis();

        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(String("https://script.google.com/macros/s/") + String(GAS_ID) + String("/exec")); // Google Script URL
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.addHeader("Content-Type", "application/json");

            String payload = 
              "{\"temperature\":" + String(temperatureC) +
              ",\"humidity\":" + String(humidity) +
              ",\"mq2Value\":" + String(mq2Value) +
              ",\"mq5Value\":" + String(mq5Value) +
              ",\"mq135Value\":" + String(mq135Value) + "}";

            Serial.println("Payload: " + payload); // Add this line to debug the payload

            int httpResponseCode = http.POST(payload);

            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println(httpResponseCode);
                Serial.println(response);
            } 
            
            else {
                Serial.print("Error on sending POST: ");
                Serial.println(httpResponseCode);
            }

            http.end(); // Free resources

        }

        // Send sensor data over LoRa
        sendLoRaMessage(temperatureC,humidity,mq2Value,mq5Value,mq135Value);

    }

    delay(1000); // Delay for 1 second for the serial print
}