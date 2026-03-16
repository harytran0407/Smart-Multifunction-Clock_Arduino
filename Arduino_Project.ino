#include <IRremote.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// ================== PIN CONFIG ==================
#define PIN_IR_RECEIVER   7
#define PIN_DHT           2
#define PIN_GAS           A0
#define PIN_TRIG          8
#define PIN_ECHO          9

#define DHT_TYPE          DHT11

// ================== CONSTANTS ==================
#define HEART_RATE_BUFFER 4
#define HEART_IR_THRESHOLD 30000
#define DATA_SEND_INTERVAL 1000

// ================== OBJECTS ==================
SoftwareSerial espSerial(10, 11);   // RX, TX
IRrecv irReceiver(PIN_IR_RECEIVER);
decode_results irResults;

DHT dht(PIN_DHT, DHT_TYPE);
MAX30105 heartSensor;

// ================== HEART RATE VARIABLES ==================
byte heartRateBuffer[HEART_RATE_BUFFER];
byte bufferIndex = 0;

long lastBeatTime = 0;
int averageBPM = 0;
float currentBPM = 0;

// ================== TIMER ==================
unsigned long lastSendTime = 0;

// ==========================================================
// INITIALIZATION
// ==========================================================
void setup() {

  Serial.begin(9600);
  espSerial.begin(9600);

  irReceiver.enableIRIn();
  dht.begin();

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  initHeartSensor();

  Serial.println("SYSTEM READY");
}

// ==========================================================
// HEART SENSOR INIT
// ==========================================================
void initHeartSensor() {

  if (!heartSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not detected");
    while (true);
  }

  heartSensor.setup();
  heartSensor.setPulseAmplitudeRed(0x0A);
  heartSensor.setPulseAmplitudeGreen(0);
  heartSensor.setPulseAmplitudeIR(0x1F);
}

// ==========================================================
// MAIN LOOP
// ==========================================================
void loop() {

  updateHeartRate();
  sendSensorData();
  handleIRRemote();
}

// ==========================================================
// HEART RATE PROCESSING
// ==========================================================
void updateHeartRate() {

  long irValue = heartSensor.getIR();

  if (irValue < HEART_IR_THRESHOLD) {
    averageBPM = 0;
    return;
  }

  if (checkForBeat(irValue)) {

    long delta = millis() - lastBeatTime;
    lastBeatTime = millis();

    currentBPM = 60 / (delta / 1000.0);

    if (currentBPM > 20 && currentBPM < 255) {
      storeHeartRate((byte)currentBPM);
    }
  }
}

void storeHeartRate(byte bpm) {

  heartRateBuffer[bufferIndex++] = bpm;
  bufferIndex %= HEART_RATE_BUFFER;

  int sum = 0;

  for (byte i = 0; i < HEART_RATE_BUFFER; i++)
    sum += heartRateBuffer[i];

  averageBPM = sum / HEART_RATE_BUFFER;
}

// ==========================================================
// SENSOR DATA TRANSMISSION
// ==========================================================
void sendSensorData() {

  if (millis() - lastSendTime < DATA_SEND_INTERVAL)
    return;

  lastSendTime = millis();

  sendTemperatureHumidity();
  sendHeartRate();
  sendGasLevel();
  sendDistance();
}

// ==========================================================
// SENSOR READ FUNCTIONS
// ==========================================================
void sendTemperatureHumidity() {

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature))
    return;

  sendData("TEMP_", temperature);
  delay(30);

  sendData("HUM_", humidity);
  delay(30);
}

void sendHeartRate() {

  sendData("BPM_", averageBPM);

  Serial.print("BPM: ");
  Serial.println(averageBPM);
}

void sendGasLevel() {

  int gasValue = analogRead(PIN_GAS);

  sendData("GAS_", gasValue);
  delay(20);
}

void sendDistance() {

  long distance = readDistance();

  sendData("DIST_", distance);
}

// ==========================================================
// ULTRASONIC SENSOR
// ==========================================================
long readDistance() {

  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);

  return duration * 0.034 / 2;
}

// ==========================================================
// IR REMOTE HANDLER
// ==========================================================
void handleIRRemote() {

  if (!irReceiver.decode(&irResults))
    return;

  unsigned long code = irResults.value;

  if (code == 0xFFFFFFFF) {
    irReceiver.resume();
    return;
  }

  switch (code) {

    case 0xFFA857: sendCommand("AL_MODE"); break;
    case 0xFFA25D: sendCommand("AL_STOP"); break;
    case 0xFF22DD: sendCommand("AL_SAVE"); break;

    case 0xFF6897: sendNumber(0); break;
    case 0xFF30CF: sendNumber(1); break;
    case 0xFF18E7: sendNumber(2); break;
    case 0xFF7A85: sendNumber(3); break;
    case 0xFF10EF: sendNumber(4); break;
    case 0xFF38C7: sendNumber(5); break;
    case 0xFF5AA5: sendNumber(6); break;
    case 0xFF42BD: sendNumber(7); break;
    case 0xFF4AB5: sendNumber(8); break;
    case 0xFF52AD: sendNumber(9); break;
  }

  irReceiver.resume();
}

// ==========================================================
// SERIAL COMMUNICATION
// ==========================================================
void sendCommand(const char* command) {

  espSerial.println(command);
  Serial.println(command);
}

void sendNumber(int number) {

  espSerial.print("NUM_");
  espSerial.println(number);
}

void sendData(const char* prefix, float value) {

  espSerial.print(prefix);
  espSerial.println(value);
}
