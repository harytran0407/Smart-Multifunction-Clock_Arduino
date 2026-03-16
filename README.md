# Smart Multifunction Clock

## Overview

Project này là một Smart Alarm Clock IoT chạy trên ESP8266

## Features

- Alarm Clock thông minh
- Cảm biến khoảng cách để tắt alarm
- Điều khiển từ xa qua Blynk
- Lưu alarm vào EEPROM
- Đo nhịp tim
- Đo nhiệt độ & độ ẩm
- Cảnh báo gas
- Hiển thị OLED + 7 Segment

## Model

Arduino Sensors - Serial (UART)
----> ESP8266 (Main Controller)
----> Blynk Cloud
----> Blynk App

## Logic Code

- Blynk Module
- Serial Module
- Sensor Module
- Display Module
- Alarm Module

## System Flow

    setup(){
        Serial.begin();
        init RTC;
        setup LED;
        setup OLED;
        setup 7 Segment Display;
        EEPROM;
        Connect Blynk;
        System Status;
    }

    loop(){
        runBlynk();
        handleSerial();
        processBpm();
        processTempAndHum();
        processGas();
        handleGas();
        processDistance();
        handleDisplay();
        handleAlarm();
        handleLed();
    }

#1. Serial Communication
   - ESP8266 nhận dữ liệu sensor từ Arduino qua UART:
   - Format message:
       TEMP_00
       HUM_00
       DIST_00
       GAS_00
       BPM_00
       AL_MODE/ AL_STOP/ AL_SAVE

   - Parser:
   - handleSerial() (đọc dữ liệu từ UART) ----> parseCmd() (phân tích và xử lý dữ liệu)
       VD: TEMP_30 ---> temp = 30

#2. Sensor Processing
   a. Temperature & Humidity
   - processTempAndHum() ---> gửi lên Blynk ---> Xử lý trạng thái

       temp < 18 → COLD
       temp > 35 → HOT
       hum < 30 → DRY
       hum > 70 → WET
       ---> updateStatus on Terminal ---> Gửi trạng thái lên Blynk (COLD/ HOT/ DRY/ WET)

   b. Gas Sensor
   - processGas() ---> handleGas()
   - gasValue > 700 ---> gasAlarm = true ---> tone(BUZZER_PIN, 2000) ---> updateStatus on Terminal ---> showGasWarning() on OLED;

   - Alarm tự tắt sau 5 phút
       if (millis() - gasStartTime > 300000) {
       gasAlarm = false; }

   - Nếu tắt bằng Ultra hoặc OFF:
       gasIgnoreUntil = millis() + 300000;
     Giữ trạng thái IGNORE trong 5 phút, sau 5 phút vẫn còn vượt mức GAS thì BUZZER tiếp tục phát.

   c. Distance Sensor
   processDistance()
   dist < 5cm ---> alarmRining, gasAlarm == false

   d. Heart Rate System
   processBpm()
   - Dùng phương pháp lọc nhiễu, sử dụng BUFFER lưu 4 số và cho ra 1 số trung bình.
   - Sau đó gửi lên Blynk, trực quan hóa bằng Chart
     processBpmAverage()
   - Tính trung bình BPM trong 30 giây, rồi cho ra kết quả trạng thái.

#3. Alarm System
   4 nhóm alarm:
   MON-FRI
   SAT-SUN
   ALL DAYS
   CUSTOM

   if
   {
   alarmEnabled
   AND weekday matched
   AND hour == alarmH
   AND minute == alarmM
   } ---> alarmRinging == true

#4. Smart Buzzer

   # Đối với Alarm Clock:
   - Buzzer tăng 300hz mỗi 500ms, không gây giật mình khi báo thức

   # Đối với Gas Alarm:
   - Buzzer bình thường

#5. Display System

    OLED có 4 MODE:
   1. Main Screen
      - Alarm Time
      - Gas Level
      - Clock
      - Date
      - Temperature
      - Humidity
   2. Heart Mode
      - Progress bar
      - Heart Animation
      - Live BPM
   3. Result Screen
      - Average BPM
      - Health Status
   4. Gas Warning
      - WARNING
      - GAS DETECTED
      - LEVEL: xxx

    Khi bắt đầu đo BPM:

   Main Screen ---> Heart Mode ---> Result Screen ---> Main Screen

    Khi có cảnh báo GAS:

   Main Screen ---> Gas Warning ---> Main Screen

   # 7 SEGMENT DISPLAY

   HH:MM and Blink input digits

6. Blynk Connection
   <img width="915" height="510" alt="Screenshot 2026-03-16 211132" src="https://github.com/user-attachments/assets/5860d9af-d205-4c5a-81cc-a04031184657" />

