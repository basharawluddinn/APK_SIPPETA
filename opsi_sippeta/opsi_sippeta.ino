// ESP32 - SIPPETA (Fix Jadwal + Hanya Kelembaban)
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ====== KONFIGURASI WIFI ======
const char* ssid = "TECNO CAMON 40 Pro 5G";
const char* password = "tanyaibucoba";

// ====== LCD I2C ======
LiquidCrystal_PCF8574 lcd(0x27);

// ====== SERVER ======
WebServer server(80);

// ====== NTP (WIB = GMT+7) ======
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// ====== PIN ======
#define SENSOR_KELEMBABAN_TANAH 36
#define POMPA 26
#define BUZZER 2

// ====== VARIABEL ======
int kelembaban_tanah = 0;
bool pumpStatus = false;
bool manualOverride = false;
bool jadwalPestisidaAktif = false;

// Threshold kelembaban (%)
const int BATAS_KELEMBABAN = 40;

// Jadwal Pestisida
int startHour = -1, startMinute = -1;
int stopHour = -1, stopMinute = -1;
int scheduleDay = -1, scheduleMonth = -1;
int durationOn = 0;

// smoothing
float smoothedKelembaban = 0.0;
const float ALPHA = 0.2;

// settle time sensor setelah pompa
unsigned long ignoreSensorUntil = 0;
const unsigned long SETTLE_MS_AFTER_SWITCH = 1200UL;

// helper baca rata-rata ADC
int readAdcAverage(int pin, int samples=10) {
  long sum = 0;
  for (int i=0;i<samples;i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return int(sum / samples);
}

// ====== BACA SENSOR ======
void readSensors() {
  if (millis() < ignoreSensorUntil) return;

  // kelembaban (%)
  int rawHum = readAdcAverage(SENSOR_KELEMBABAN_TANAH, 12);
  float humPerc = (float)rawHum / 4095.0f * 100.0f;
  humPerc = 100.0f - humPerc; // invert
  if (smoothedKelembaban == 0.0) smoothedKelembaban = humPerc;
  else smoothedKelembaban = ALPHA * humPerc + (1 - ALPHA) * smoothedKelembaban;
  kelembaban_tanah = int(smoothedKelembaban + 0.5);
}

// ====== LCD ======
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hmb:");
  lcd.print(kelembaban_tanah);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Pompa:");
  lcd.print(pumpStatus ? "ON " : "OFF");
}

// ====== LOGIKA JADWAL ======
bool isScheduleActiveNow(int curHour, int curMinute, int curDay, int curMonth) {
  if (startHour < 0 || startMinute < 0) return false;
  if (scheduleDay > 0 && scheduleMonth > 0) {
    if (!(curDay == scheduleDay && curMonth == scheduleMonth)) return false;
  }

  int curMinOfDay = curHour * 60 + curMinute;
  int startMin = startHour * 60 + startMinute;
  int stopMin = stopHour * 60 + stopMinute;

  if (stopMin > startMin) {
    return (curMinOfDay >= startMin && curMinOfDay < stopMin);
  } else {
    return (curMinOfDay >= startMin || curMinOfDay < stopMin);
  }
}

// ====== LOGIKA POMPA ======
void updatePumpLogic() {
  timeClient.update();
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  time_t epoch = timeClient.getEpochTime();
  struct tm * now = gmtime(&epoch);
  int curDay = now->tm_mday;
  int curMonth = now->tm_mon + 1;

  bool jadwalSekarang = isScheduleActiveNow(h, m, curDay, curMonth);
  jadwalPestisidaAktif = jadwalSekarang;

  if (manualOverride) {
    digitalWrite(POMPA, LOW);
    pumpStatus = true;
    digitalWrite(BUZZER, LOW);
    ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
  } else if (jadwalPestisidaAktif) {
    digitalWrite(POMPA, LOW);
    pumpStatus = true;
    digitalWrite(BUZZER, LOW);
    ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
  } else {
    if (kelembaban_tanah < BATAS_KELEMBABAN) {
      digitalWrite(POMPA, LOW);
      digitalWrite(BUZZER, HIGH);
      if (!pumpStatus) ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
      pumpStatus = true;
      Serial.println("PERINGATAN: Tanah kering!");
    } else {
      digitalWrite(POMPA, HIGH);
      digitalWrite(BUZZER, LOW);
      if (pumpStatus) ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
      pumpStatus = false;
    }
  }
}

// ====== ENDPOINT ======
void sendJSON(String json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleStatus() {
  readSensors();
  updatePumpLogic();
  updateLCD();

  StaticJsonDocument<512> doc;
  doc["kelembaban"] = kelembaban_tanah;
  doc["pump"] = pumpStatus;
  doc["mode_manual"] = manualOverride;
  doc["jadwal_pestisida"] = jadwalPestisidaAktif;

  String out;
  serializeJson(doc, out);
  sendJSON(out);
}

void handlePumpOn() {
  manualOverride = true;
  digitalWrite(POMPA, LOW);
  digitalWrite(BUZZER, LOW);
  pumpStatus = true;
  ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
  sendJSON("{\"pump\":\"on\"}");
}

void handlePumpOff() {
  manualOverride = false;
  digitalWrite(POMPA, HIGH);
  digitalWrite(BUZZER, LOW);
  pumpStatus = false;
  ignoreSensorUntil = millis() + SETTLE_MS_AFTER_SWITCH;
  sendJSON("{\"pump\":\"off\"}");
}

void handlePumpSchedule() {
  if (server.method() == HTTP_POST && server.hasArg("plain")) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) { sendJSON("{\"error\":\"JSON salah\"}"); return; }

    String date = doc["date"] | "";
    String startTime = doc["startTime"] | "";
    String endTime = doc["endTime"] | "";
    durationOn = doc["duration"] | 0;

    if (date.length() < 10 || startTime.length() < 4) {
      sendJSON("{\"error\":\"format input salah\"}"); return;
    }

    scheduleDay = date.substring(8, 10).toInt();
    scheduleMonth = date.substring(5, 7).toInt();
    startHour = startTime.substring(0, 2).toInt();
    startMinute = startTime.substring(3, 5).toInt();

    if (endTime.length() >= 4) {
      stopHour = endTime.substring(0, 2).toInt();
      stopMinute = endTime.substring(3, 5).toInt();
    } else {
      int startTotal = startHour * 60 + startMinute;
      int stopTotal = startTotal + durationOn;
      stopHour = (stopTotal / 60) % 24;
      stopMinute = stopTotal % 60;
    }

    Serial.printf("Jadwal disimpan: %02d-%02d %02d:%02d -> %02d:%02d (%d menit)\n",
                  scheduleDay, scheduleMonth, startHour, startMinute, stopHour, stopMinute, durationOn);

    sendJSON("{\"status\":\"jadwal pestisida disimpan\"}");
  } else {
    sendJSON("{\"error\":\"Gunakan POST\"}");
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  pinMode(POMPA, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(POMPA, HIGH);
  digitalWrite(BUZZER, LOW);

  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.print("SIPPETA Ready");

  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  server.on("/status", handleStatus);
  server.on("/pump/on", handlePumpOn);
  server.on("/pump/off", handlePumpOff);
  server.on("/pump/schedule", handlePumpSchedule);
  server.begin();
}

// ====== LOOP ======
void loop() {
  server.handleClient();
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 3000) {
    lastUpdate = millis();
    timeClient.update();
    readSensors();
    updatePumpLogic();
    updateLCD();
  }
}
