#include <StaticThreadController.h>
#include <Thread.h>
#include <ThreadController.h>
#include "thingProperties.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Firebase.h>

#define relayPin 8
#define SOIL_HUMIDITY_THRESHOLD 50
#define FIREBASE_HOST "https://projet-iot-ci2-default-rtdb.europe-west1.firebasedatabase.app/"
Firebase fb(FIREBASE_HOST);

#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastFirebasePushTime = 0;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long firebaseInterval = 1;
const unsigned long displayInterval = 500;

bool previousRelayState = false; 

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi déconnecté! Tentative de reconnexion...");
    unsigned long startAttemptTime = millis();
    const unsigned long connectionTimeout = 10000;

    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < connectionTimeout) {
      WiFi.begin(SSID, PASS);
      delay(50);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connecté avec succès!");
    } else {
      Serial.println("\nÉchec de connexion au WiFi. Nouvelle tentative dans la boucle principale.");
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(1500);
  Serial.println("Starting setup...");

  pinMode(relayPin, OUTPUT);

  Serial.println("Scanning I2C devices...");
  Wire.begin();
  scanI2C();

  Serial.println("Initializing LCD...");
  lcd.init();
  delay(100);
  lcd.backlight();
  lcd.print("Initializing...");
  Serial.println("LCD initialized successfully!");

  Serial.println("Initializing DHT11...");
  dht.begin();
  Serial.println("DHT11 initialized successfully!");

  Serial.println("Initializing IoT Cloud...");
  initProperties();

  Serial.println("Connecting to WiFi...");
  reconnectWiFi();

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(4);
  ArduinoCloud.printDebugInfo();
  ArduinoCloud.update();
}

void loop() {

  bool currentRelayState = fb.getBool("Relay/state");
  if (currentRelayState != previousRelayState) {

    Serial.println("Relay state changed! Updating...");
    previousRelayState = currentRelayState;

    if (currentRelayState) {
      digitalWrite(relayPin, HIGH);
      Serial.println("Relay turned ON");
    } else {
      digitalWrite(relayPin, LOW);
      Serial.println("Relay turned OFF");
    }
    
    return;
  }

  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi déconnecté! Tentative de reconnexion...");
    reconnectWiFi();
  }

  ArduinoCloud.update();

  
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  sensor = map(analogRead(A0), 1023, 0, 0, 100);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error reading DHT11!");
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!   ");
    return;
  }

  
  if (millis() - lastDisplayUpdateTime >= displayInterval) {
    lastDisplayUpdateTime = millis();

    lcd.setCursor(0, 0);
    lcd.print("H:");
    lcd.print(humidity, 1);
    lcd.print("% T:");
    lcd.print(temperature, 1);
    lcd.print("C ");

    lcd.setCursor(0, 1);
    lcd.print("Soil:");
    lcd.print(sensor);
    lcd.print("%       ");
  }


  if (millis() - lastFirebasePushTime >= firebaseInterval) {
    lastFirebasePushTime = millis();
    onRelayChange();

    fb.pushInt("Hum_Air/value", humidity);
    Serial.println("Air Humidity sent to Firebase.");
    onRelayChange();

    fb.pushInt("Temperature_air/value", temperature);
    Serial.println("Air Temp sent to Firebase.");
    onRelayChange();

    fb.pushInt("Hum_Sol/value", sensor);
    Serial.println("Soil Humidity sent to Firebase.");
    onRelayChange();
  }

  onAutoIrrigationChange();
  onSensorChange();
}

void onSensorChange() {
  int valeurBrute = analogRead(A0);
  sensor = map(valeurBrute, 1023, 0, 0, 100);
  Serial.print("Soil Humidity: ");
  Serial.println(sensor);
}

void scanI2C() {
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("I2C scan completed successfully!");
}

void onHumidityChange(){
  
}

void onTemperatureChange(){
  
}

void onRelayChange() {
  bool firebaseRelayState = fb.getBool("Relay/state");
  bool actualRelayState = digitalRead(relayPin);

  if (firebaseRelayState && !actualRelayState) {
    Serial.println("Relay state mismatch detected: Firebase ON, actual OFF. Correcting...");
    digitalWrite(relayPin, HIGH);
  } else if (!firebaseRelayState && actualRelayState) {
    Serial.println("Relay state mismatch detected: Firebase OFF, actual ON. Correcting...");
    digitalWrite(relayPin, LOW);
  } else {
    Serial.println("Relay state is consistent.");
  }
}

void onAutoIrrigationChange() {
  autoIrrigation = fb.getBool("Auto/state");
  
  if (autoIrrigation) {
  Serial.print("Auto Mode is ON");
    if (sensor <= SOIL_HUMIDITY_THRESHOLD) {
      digitalWrite(relayPin, HIGH);
    } else {
      digitalWrite(relayPin, LOW);
    }
  }
}