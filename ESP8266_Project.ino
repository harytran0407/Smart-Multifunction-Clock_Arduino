// ================== BLYNK CONFIG ==================
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6zHbbWcIu"
#define BLYNK_TEMPLATE_NAME "Progress 1"
#define BLYNK_AUTH_TOKEN "0_SQ8u9mwXd82VF2gGt3rZrNlhLdtyUi"

// ================== LIBRARY ==================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TM1637Display.h>
#include <WidgetTerminal.h>
#include <WidgetRTC.h>
#include <TimeLib.h>
#include <EEPROM.h>

// ================== WIFI ==================
char ssid[] = "Tran Hung";
char pass[] = "quochung2006";

// ================== RTC ==================
RTC_DS3231 rtc;
WidgetRTC blynkRTC;

// ================== OLED ==================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================== DHT11 ==================
float temp = 0;
float hum = 0;

//================== ULTRA SONIC ==================
float dist = 0;

// ================== GAS SENSOR ==================
int gasValue = 0;
bool gasAlarm = false;              // Gas alarm active
unsigned long gasStartTime = 0;     // Alarm start time
unsigned long gasIgnoreUntil = 0;   // Ignore period to avoid spam

// ================== HEART RATE ==================
bool heartMode = false;          // Heart measurement active
bool heartBeatState = false;

int bpm = 0;
int BPMs = 0;
int finalBPM = 0;
unsigned long lastBeat = 0;
unsigned long lastBPMReceive = 0;

//================== BPM 30s AVERAGE ==================
#define BPM_AVG_TIME 30000   // measure for 30 seconds
#define BPM_REST_TIME 5000   // rest time before next measure

unsigned long bpmStartTime = 0;
unsigned long bpmRest = 0;

long bpmSum = 0;
int bpmCount = 0;

bool showResultScreen = false;
unsigned long resultStart = 0;

String bpmStatus = "";

//================== LED ==================
#define LED_PIN 14   // D5
unsigned long ledOnTime = 0;
bool ledState = false;

// ================== TM1637 ==================
#define TM_CLK 16
#define TM_DIO 2
TM1637Display display(TM_CLK, TM_DIO);

//================== REMOTE ==================
bool alarmSetMode = false;      // Alarm time input mode

String alarmInput = "";         // Remote input buffer
String uartBuffer = "";         // UART receive buffer

unsigned long lastBlink = 0;
bool blinkState = false;

unsigned long lastInputTime = 0;

#define SET_TIMEOUT 10000       // Exit set mode if idle

// ================== BUZZER ==================
#define BUZZER_PIN 13

// ================== TERMINAL ==================
WidgetTerminal terminal(V8);

// ================== ALARM CORE ==================
int alarmH = -1;
int alarmM = -1;

bool alarmEnabled = false;
bool alarmRinging = false;

int lastAlarmMinute = -1;

unsigned long alarmStart = 0;

// Weekday mask
// 1 = Monday ... 7 = Sunday
bool weekdayNum[8] = {0};

// ================== SMART BUZZER ==================
unsigned long lastBeep = 0;
int beepFreq = 1000;

// ================== TIME STORAGE (BLYNK INPUT) ==================
// Monday → Friday
int mfHour = -1;
int mfMinute = -1;

// Saturday → Sunday
int ssHour = -1;
int ssMinute = -1;

// All days
int allHour = -1;
int allMinute = -1;

// Custom
int cusHour = -1;
int cusMinute = -1;

// ================== STATUS ==================
String lastStatus = "";


//=======================================================================
//============================== BLYNK MODULE ===========================
//=======================================================================
BLYNK_CONNECTED() {
  blynkRTC.begin();
  setSyncInterval(60);
  updateStatus("======= SYSTEM READY ======");
}

void runBlynk() {
  Blynk.run();
  static unsigned long lastSync = 0;
  if (millis() - lastSync > 60000) {
    syncRTC();
    lastSync = millis();
  }
}

// ================== RTC SYNC FROM INTERNET ==================
void syncRTC() {
  if (year() > 2000) {   
    rtc.adjust(DateTime(year(), month(), day(), hour(), minute(), second()));
    
  }
}

//================== TIME INPUT ==================
BLYNK_WRITE(V0) { // MON-FRI
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    mfHour = t.getStartHour();
    mfMinute = t.getStartMinute();
    updateStatus("TIME MON-FRI: " + String(mfHour) + ":" + String(mfMinute));
  }
}

BLYNK_WRITE(V2) { // SAT-SUN
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    ssHour = t.getStartHour();
    ssMinute = t.getStartMinute();
    updateStatus("TIME SAT-SUN: " + String(ssHour) + ":" + String(ssMinute));
  }
}

BLYNK_WRITE(V4) { // ALL DAYS
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    allHour = t.getStartHour();
    allMinute = t.getStartMinute();
    updateStatus("TIME ALL DAYS: " + String(allHour) + ":" + String(allMinute));
  }
}

BLYNK_WRITE(V6) { // UP TO YOU
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    cusHour = t.getStartHour();
    cusMinute = t.getStartMinute();
    updateStatus("TIME CUSTOM: " + String(cusHour) + ":" + String(cusMinute));
  }
}
//================== SET BUTTONS ==================
BLYNK_WRITE(V1) { // MON-FRI
  if (!param.asInt()) return;
  Blynk.syncVirtual(V0);   //Blynk gửi lại giờ
  if (mfHour < 0) return;
  alarmH = mfHour; alarmM = mfMinute;
  clearMask(); 
  for (int i = 1; i <= 5; i++) weekdayNum[i] = true;
  alarmEnabled = true; lastAlarmMinute = -1;
  updateStatus(  "Set alarm for MON - FRI: " +
  String(alarmH) + ":" +
  (alarmM < 10 ? "0" : "") + String(alarmM));
  saveAlarm();
}

BLYNK_WRITE(V3) { // SAT-SUN
  if (!param.asInt()) return;
  Blynk.syncVirtual(V2);   //Blynk gửi lại giờ
  if (ssHour < 0) return;
  alarmH = ssHour; alarmM = ssMinute;
  clearMask(); 
  weekdayNum[6] = weekdayNum[7] = true;
  alarmEnabled = true; lastAlarmMinute = -1;
  updateStatus("Set alarm for SAT - SUN: " +  String(alarmH) + ":" +  (alarmM < 10 ? "0" : "") + String(alarmM));
  saveAlarm();
}

BLYNK_WRITE(V5) { // ALL DAYS
  if (!param.asInt()) return;
  Blynk.syncVirtual(V4);   //Blynk gửi lại giờ
  if (allHour < 0) return;
  alarmH = allHour; alarmM = allMinute;
  clearMask(); 
  for (int i = 1; i <= 7; i++) weekdayNum[i] = true;
  alarmEnabled = true; lastAlarmMinute = -1;
  updateStatus("Set alarm for ALL DAYS: " +  String(alarmH) + ":" +  (alarmM < 10 ? "0" : "") + String(alarmM));
  saveAlarm();
}

BLYNK_WRITE(V7) { // CUSTOM
  if (!param.asInt()) return;
  Blynk.syncVirtual(V6);   //Blynk gửi lại giờ
  if (cusHour < 0) return;
  alarmH = cusHour; alarmM = cusMinute;
  clearMask(); 
  DateTime now = rtc.now();
  int today = now.dayOfTheWeek();  
  if (today == 0) today = 7;  // Sunday -> 7
  weekdayNum[today] = true;
  alarmEnabled = true; lastAlarmMinute = -1;
  updateStatus("Set alarm (CUSTOM): " +  String(alarmH) + ":" +  (alarmM < 10 ? "0" : "") + String(alarmM));
  saveAlarm();
}

// ================== STOP BUTTON ==================
BLYNK_WRITE(V9) {
  if (!param.asInt()) return;

  if (alarmRinging || gasAlarm) {
    noTone(BUZZER_PIN);
    alarmRinging = false;
    gasAlarm = false;
    gasIgnoreUntil = millis()+300000; //ignore gas 5 min
    lastAlarmMinute = alarmM;
    updateStatus("BUZZER STOPPED");
  }
  
}


//=======================================================================================
//======================================== SET UP =======================================
//=======================================================================================
void setup() {
  //Serial begin
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);

  // SDA = GPIO4 (D2), SCL = GPIO5 (D1)
  Wire.begin(4, 5);
  // DS3231 RTC
  rtc.begin();
  
  // ================== SETUP LED ==================
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // ============== SETUP OLED =================
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  Serial.println("OLED not found");
  while (true);
  }
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  oled.display();
  
  // ============== SETUP 7 SEGMENT DISPLAY =================
  display.setBrightness(7);
  clearMask();
  
  // ================== EEPROM ==================
  EEPROM.begin(64);
  loadAlarm();

  // ================== BLYNK CONNECTION ==================
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // ================== SYSTEM STATUS ==================
  updateStatus("SYSTEM READY");

  
}

//================================================================================================
//============================================= LOOP =============================================
//================================================================================================

void loop() {
  runBlynk();

  handleSerial();   // đọc UART trước

  //BPM
  if (millis() - lastBPMReceive > 3000) {
    bpm = 0;
    BPMs = 0; //Sau 3 giây không có dữ liệu thì reset BPM
  }

  processBpm();
  processBpmAverage();

  //Temp & Hum
  processTempAndHum();

  //Gas
  processGas();

  //Distance
  processDistance();

  //Display
  handleDisplay();

  //Alarm
  handleAlarm();

  //Gas Alarm
  handleGas();

  //LED
  handleLed();
  
}

//===========================================================================
//================================ SERIAL MODULE ============================
//===========================================================================

//-------------------- PARSE CMD -------------------

void parseCmd(String cmd) {

    // ====== NHẬN NHỊP TIM ======
    if (cmd.startsWith("BPM_")) {
    BPMs = cmd.substring(4).toInt();
    lastBPMReceive = millis();
    return;
    }
  
    // ====== NHẬN NHIỆT ĐỘ TỪ ARDUINO ======
    if (cmd.startsWith("TEMP_")) {
    temp = cmd.substring(5).toFloat(); 
    //updateStatus("TEMP: " + String(temp) + " C");
    return;
    }

    // ====== NHẬN ĐỘ ẨM ======
    if (cmd.startsWith("HUM_")) {
    hum = cmd.substring(4).toFloat();
    //updateStatus("HUM: " + String(hum) + " %");
    return;
    }

    // ====== NHẬN ULTRASONIC TỪ ARDUINO ======
    if (cmd.startsWith("DIST_")) {
    dist = cmd.substring(5).toFloat();
    //updateStatus("DIST: "+ String(dist));    
    return;
    }
  
    
    // ====== NHẬN GAS ======
    if (cmd.startsWith("GAS_")) {
    gasValue = cmd.substring(4).toInt();
    return;
  }
  
  // ====== NHẬN REMOTE TỪ ARDUINO ======
  if (cmd == "AL_MODE") {
    alarmSetMode = true;
    alarmInput = "";
    lastInputTime = millis();
    display.clear();
    updateStatus("REMOTE: SET MODE");
    return;
  }

  if (cmd == "AL_STOP") {
    if (alarmSetMode) {
      alarmInput = "";
      display.clear();
      updateStatus("REMOTE: INPUT CLEARED");
    } else {
      alarmRinging = false;
      gasAlarm = false;
      noTone(BUZZER_PIN);
      gasIgnoreUntil = millis() + 300000;
      updateStatus("REMOTE: ALARM STOP");
    }
    return;
  }

  if (cmd.startsWith("NUM_") && alarmSetMode) {
    lastInputTime = millis();
    if (alarmInput.length() < 4) {
      alarmInput += cmd.charAt(4);
    }
    return;
  }

  if (cmd == "AL_SAVE") {

    if (!alarmSetMode || alarmInput.length() != 4) {
      updateStatus("REMOTE: INVALID SAVE");
      return;
    }

    int h = alarmInput.substring(0, 2).toInt();
    int m = alarmInput.substring(2, 4).toInt();

    if (h > 23 || m > 59) {
      updateStatus("REMOTE: TIME ERROR");
      alarmInput = "";
      return;
    }

    alarmH = h;
    alarmM = m;
    alarmEnabled = true;
    lastAlarmMinute = -1;

    clearMask();
    for (int i = 1; i <= 7; i++) weekdayNum[i] = true;

    saveAlarm();

    alarmSetMode = false;
    alarmInput = "";
    updateStatus("REMOTE: ALARM SAVED");
  }
}
//-------------------- TERMINAL -------------------
void updateStatus(String s) {
  if (s != lastStatus) {
    Serial.println(s);
    terminal.println(s);
    terminal.flush();
    lastStatus = s;
  }
}

//-------------------- SERIAL -------------------
void handleSerial() {
  //Read data from Arduino
  while (Serial.available()) {
    char c = Serial.read();//Đọc theo chiều dọc
    if (c == '\n') { //Kết thúc câu lệnh
      parseCmd(uartBuffer); //Tiến hành xử lý câu lệnh
      uartBuffer = "";
    } 
    else if (c != '\r') {//Đọc tiếp
      uartBuffer += c;
    }
  }
} 

//================================================================================
//================================ SENSOR MODULE =================================
//================================================================================

//================== FUNCTION DIST ====================

void processDistance(){

  if (dist < 5 && (alarmRinging || gasAlarm)){

    alarmRinging = false;
    gasAlarm = false;

    noTone(BUZZER_PIN);

    gasIgnoreUntil = millis() + 300000;// Gas ignore trong 5 phút từ sau khi OFF

    updateStatus("DIST SENSOR: ALARM STOPPED");
  }

}


//================== FUNCTION GAS =====================

void processGas(){

  Blynk.virtualWrite(V16, gasValue);

  if (millis() < gasIgnoreUntil) return; //Còn trong thời gian ignore thì không xử lý

  static int gasCount = 0;
  if (gasValue > 700) {
    gasCount++;
    if (gasCount >= 3 && !gasAlarm) {
      gasAlarm = true;
      gasStartTime = millis();
      tone(BUZZER_PIN, 2000);
      updateStatus("WARNING: GAS DETECTED!");
      }
  } else {
   gasCount = 0;
  }
}

//================== FUNCTION TEMP AND HUM ===================

void processTempAndHum(){
  //Send Blynk
  Blynk.virtualWrite(V10,temp);
  Blynk.virtualWrite(V11,hum);
  
  //Mỗi lần check cách nhau 5 giây
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();

  //Status of Temp and Hum
  String tempStatus = "NORMAL";
  String humStatus = "NORMAL";
  //Temperature
  if (temp < 18){
    updateStatus("Temperature: COLD");
    tempStatus = "COLD";
  } else if (temp > 35){
    updateStatus("Temperature: HOT");
    tempStatus = "HOT";
  }
  //Humidity 
  if (hum < 30){
    updateStatus("Humidity: DRY");
    humStatus = "DRY";
  } else if (hum > 70){
    updateStatus("Humidity: WET");
    humStatus = "WET";
  }
  //Send to Blynk
  Blynk.virtualWrite(V12,tempStatus);
  Blynk.virtualWrite(V13,humStatus);
}

//================== FUNCTION BPM =====================


// ------ PROCESS BPM ------
void processBpm(){

  // chỉ validate thôi
  if (BPMs > 20 && BPMs < 200) {
    bpm = BPMs;

    // dùng cho tính trung bình 30s
    if (millis() > bpmRest) {

      if (bpmStartTime == 0){
        heartMode = true;
        bpmStartTime = millis();
      }

      bpmSum += bpm;
      bpmCount++;
    }

    Blynk.virtualWrite(V14, bpm);
  }
  else {
    bpm = 0;
  }
}

void processBpmAverage() {

  if (bpmStartTime == 0) return;

  if (millis() - bpmStartTime < BPM_AVG_TIME) return;

  if (bpmCount == 0) return;

  //Average BPM after 30s
  int avgBPM = bpmSum / bpmCount;
  
  Blynk.virtualWrite(V17,avgBPM);
  finalBPM = avgBPM;
  showResultScreen = true;
  resultStart = millis();
  String newStatus;

  if (avgBPM < 40) newStatus = "ABNORMAL (Too Low)!";
  else if (avgBPM > 100) newStatus = "ABNORMAL (Too High)!";
  else newStatus = "NORMAL";

  updateStatus("AVG BPM: " + String(avgBPM));
  updateStatus("Status: "+ newStatus);
  
  if (newStatus != bpmStatus) {
    bpmStatus = newStatus;
    Blynk.virtualWrite(V15, bpmStatus);
  }

  digitalWrite(LED_PIN, HIGH);
  ledOnTime = millis();
  ledState = true;

  // Reset
  //bpmStartTime = 0;
  bpmSum = 0;
  bpmCount = 0;
  bpmRest = millis() + BPM_REST_TIME;
}



//================================================================================
//=============================== DISPLAY MODULE =================================
//================================================================================


//================== FUNCTION LED =================

void handleLed() {
  if (ledState && millis() - ledOnTime >= 5000) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
  }
}


//================== FUNCTION OLED ==================

// ------ MAIN SCREEN ------
void updateOLED(int h, int m) {

  oled.clearDisplay();
  oled.setTextColor(WHITE);

  DateTime now = rtc.now();

  int d = now.day();
  int mo = now.month();
  int y = now.year() % 100;
  int wd = now.dayOfTheWeek();

  String weekDay[7] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  //TOP BAR
  oled.setTextSize(1);

  // Alarm
  oled.setCursor(0,0);
  oled.print("A ");
  if (alarmEnabled) {
    if (alarmH < 10) oled.print("0");
    oled.print(alarmH);
    oled.print(":");
    if (alarmM < 10) oled.print("0");
    oled.print(alarmM);
  } else {
    oled.print("--:--");
  }
  //Gas
  oled.setCursor(90,0);
  oled.print("G:");
  oled.print(gasValue);
  


  //BIG CLOCK
  oled.setTextSize(3);
  String timeStr = "";

  if (h < 10) timeStr += "0";
  timeStr += String(h);
  timeStr += ":";

  if (m < 10) timeStr += "0";
  timeStr += String(m);

  //Canh giữa màn hình
  int16_t x1, y1;
  uint16_t w, h2;
  oled.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h2);
  int x = (128 - w) / 2;

  oled.setCursor(x,16);
  oled.print(timeStr);


  //DATE
  oled.setTextSize(1);
  oled.setCursor(28,42);
  oled.print(weekDay[wd]);
  oled.print(" ");

  if (d < 10) oled.print("0");
  oled.print(d);
  oled.print("/");

  if (mo < 10) oled.print("0");
  oled.print(mo);
  oled.print("/");
  oled.print(y);


  //BOTTOM INFO

  // Temperature
  oled.setCursor(0,56);
  oled.print("T:");
  oled.print(temp,0);
  oled.print((char)247);
  oled.print("C");
  //Humidity
  oled.setCursor(90,56);
  oled.print("H:");
  oled.print(hum,0);
  oled.print("%");

  

  oled.display();
}

// ------ HEART SCREEN ------
void showHeartMode() {

  unsigned long t = millis() - bpmStartTime;
  if (t < 30000) {
    showHeartScreen();
  }
  else if (showResultScreen) {
    showAvgScreen();
    if (millis() - resultStart > 10000) {
      showResultScreen = false;
      heartMode = false;
      bpmStartTime = 0;

    }
  }
}

void showHeartScreen() {

  oled.clearDisplay();

  //PROGRESS BAR
  drawProgressBar(bpmStartTime, 30000);

  //HEART ANIMATION
  int size = 3;

  if (bpm > 0) {
    int beatDelay = 30000 / bpm;
    if (millis() - lastBeat > beatDelay) {
      heartBeatState = !heartBeatState;
      lastBeat = millis();
    }
    if (heartBeatState){
      size = 4;
      drawHeart(10,23,size);
    }
    else{
      size = 3;
      drawHeart(14,27,size);
    }
  }
  //BIG BPM
  oled.setTextSize(3);
  oled.setCursor(49,25);
  oled.print(BPMs);
  
  oled.setTextSize(2);
  oled.setCursor(88,28);
  oled.print("BPM");

 
  oled.display();
}
void drawHeart(int x, int y, int s) {

  int heart[8][8] = {
    {0,1,1,0,0,1,1,0},
    {1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,0,0},
    {0,0,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}
  };

  for (int i=0;i<8;i++){
    for (int j=0;j<8;j++){
      if (heart[i][j])
        oled.fillRect(x+j*s,y+i*s,s,s,WHITE);
    }
  }
}
void drawProgressBar(unsigned long startTime, int duration) {

  int barWidth = map(millis() - startTime, 0, duration, 0, 128);

  if (barWidth > 128) barWidth = 128;

  oled.fillRect(0, 0, barWidth, 6, WHITE);
}



// ------ RESULTS BPM SCREEN ------
void showAvgScreen() {

  oled.clearDisplay();

  oled.setTextSize(1);
  oled.setCursor(28,10);
  oled.print("MEASUREMENT DONE");

  

  oled.setCursor(5,25);
  oled.print("Average: ");
  oled.print(finalBPM);
  oled.print(" BPM");

  oled.setCursor(5, 35);
  oled.print("Status: ");
  oled.print(bpmStatus);
 

  oled.display();
}

// ------ GAS WARNING SCREEN ------
void showGasWarning() {

  oled.clearDisplay();

  oled.setTextSize(2);
  oled.setCursor(15, 5);
  oled.print("WARNING");

  oled.setTextSize(1);
  oled.setCursor(35, 30);
  oled.print("GAS DETECTED");

  oled.setCursor(40, 45);
  oled.print("LEVEL:");
  oled.print(gasValue);

  oled.display();
}

//================== 7 SEGMENT =================

void showSetTime(String buf, bool blinkOn) {
  uint8_t segs[4] = {0, 0, 0, 0};

  for (int i = 0; i < 4; i++) {
    if (i < buf.length()) {
      segs[i] = display.encodeDigit(buf.charAt(i) - '0');
    }
    else if (i == buf.length()) {
      segs[i] = blinkOn ? 0 : display.encodeDigit(0);
    }
  }

  display.setSegments(segs);
}

//HH:MM
void updateClockDisplay(int h, int m) {

  DateTime now = rtc.now();
  bool blink = now.second() % 2; // nhấp nháy theo giây
  
  uint8_t seg[] = {
    //H1H2:M1M2
    display.encodeDigit(h / 10), //H1
    display.encodeDigit(h % 10) | (blink ? 0x80 : 0x00), //H2
    display.encodeDigit(m / 10), //M1
    display.encodeDigit(m % 10) //M2

  };

  display.setSegments(seg);
}

//================== HANDLE DISPLAY =================

void handleDisplay() {

  DateTime now = rtc.now();
  int h = now.hour();
  int m = now.minute();

  if (alarmSetMode) {
    handleAlarmSetMode();
    return;
  }
  if (gasAlarm) {
    showGasWarning();
  }
  else if (heartMode) {
    showHeartMode();
  }
  else {
    updateOLED(h, m);
  }
  updateClockDisplay(h, m);
}

//=================================================================================
//================================== ALARM MODULE =================================
//=================================================================================

//================== SETMODE ALARM =================

void handleAlarmSetMode() {

  if (millis() - lastBlink > 400) {
    lastBlink = millis();
    blinkState = !blinkState;
  }
  if (millis() - lastInputTime > SET_TIMEOUT) {
    alarmSetMode = false;
    alarmInput = "";
    updateStatus("REMOTE: TIMEOUT");
    return;
  }
  showSetTime(alarmInput, blinkState);
}

//================== HANDLE ALARM =================
void handleAlarm() {

  DateTime now = rtc.now();
  int h = now.hour();
  int m = now.minute();
  int today = now.dayOfTheWeek();
  if (today == 0) today = 7;

  if (alarmEnabled && weekdayNum[today] && h == alarmH && m == alarmM && lastAlarmMinute != m) {
    alarmRinging = true;
    alarmStart = millis();
    lastAlarmMinute = m;
    beepFreq = 1000;
    lastBeep = 0;
    updateStatus("ALARM RINGING!");
  }
  handleSmartBuzzer();
}

// ================== CLEAR WEEKDAY MASK ==================
void clearMask() {
  for (int i = 1; i <= 7; i++) weekdayNum[i] = false;
}

// ================== SAVE ALARM ==================
void saveAlarm() {
  EEPROM.write(0, alarmH);
  EEPROM.write(1, alarmM);
  EEPROM.write(2, alarmEnabled);

  for (int i = 1; i <= 7; i++) {
    EEPROM.write(2 + i, weekdayNum[i]);
  }

  EEPROM.commit();
  updateStatus("ALARM SAVED");
}

// ================== LOAD ALARM ==================
void loadAlarm() {
  alarmH = EEPROM.read(0);
  alarmM = EEPROM.read(1);
  alarmEnabled = EEPROM.read(2);

  for (int i = 1; i <= 7; i++) {
    weekdayNum[i] = EEPROM.read(2 + i);
  }

  if (alarmEnabled && alarmH >= 0 && alarmM >= 0) {
    updateStatus("ALARM RESTORED");
  }
}

//================== SMART BUZZER =================
void handleSmartBuzzer() {
  if (!alarmRinging) {
    noTone(BUZZER_PIN);
    return;
  }
  if (millis() - lastBeep > 500) {
    tone(BUZZER_PIN, beepFreq);
    beepFreq += 300;
    if (beepFreq > 3000)
      beepFreq = 3000;
    lastBeep = millis();
  }
  if (millis() - alarmStart > 30000) {
    noTone(BUZZER_PIN);
    alarmRinging = false;
    updateStatus("ALARM AUTO STOP");
  }
}

//================== GAS ALARM =================
void handleGas() {
  if (!gasAlarm) return;
  tone(BUZZER_PIN, 2000);
  if (millis() - gasStartTime > 300000) {
    gasAlarm = false;
    noTone(BUZZER_PIN);
    updateStatus("GAS ALARM AUTO STOP");
  }
}
