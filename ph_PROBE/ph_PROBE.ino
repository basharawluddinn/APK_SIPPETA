#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

// ====== KONFIGURASI WIFI ======
const char* ssid = "TECNO CAMON 40 Pro 5G";
const char* password = "tanyaibucoba";

WebServer server(80);

// ====== SENSOR pH ======
#define SENSOR_PH 34
float phMeter = 0;

// ====== Kalibrasi pH ======
float volt_pH7 = 2.51;   // hasil ukur saat larutan pH 7
float volt_pH4 = 3.00;   // hasil ukur saat larutan pH 4

// ====== LCD I2C ======
LiquidCrystal_PCF8574 lcd(0x27);

// ====== Rolling Text ======
String rekomendasiList[10];
int rekomendasiCount = 0;
int currentIndex = 0;
unsigned long lastScroll = 0;

// ====== FILTER EMA ======
float filteredVoltage = 0;
const float alpha = 0.05;  // makin kecil = makin halus

// ====== BACA SENSOR (dengan filter & kalibrasi) ======
void readSensorPH() {
  const int sampleCount = 100;
  long totalADC = 0;

  for (int i = 0; i < sampleCount; i++) {
    totalADC += analogRead(SENSOR_PH);
    delay(2);
  }

  float avgADC = totalADC / (float)sampleCount;
  float voltage = avgADC * (3.3 / 4095.0);

  // ====== Exponential Moving Average ======
  filteredVoltage = alpha * voltage + (1 - alpha) * filteredVoltage;

  // ====== Kalibrasi 2 titik ======
  float slope = (7.0 - 4.0) / (volt_pH7 - volt_pH4);
  float intercept = 7.0 - slope * volt_pH7;

  float newPH = slope * filteredVoltage + intercept;

  if (newPH < 0) newPH = 0;
  if (newPH > 14) newPH = 14;

  // Simpan ke global
  phMeter = round(newPH * 10.0) / 10.0;  // 1 angka belakang koma
}

// ====== RESET REKOMENDASI ======
void clearRekomendasi() {
  for (int i = 0; i < 10; i++) rekomendasiList[i] = "";
  rekomendasiCount = 0;
  currentIndex = 0;
}

// ====== ISI REKOMENDASI TANAMAN ======
void rekomendasiTanaman(JsonArray arr) {
  clearRekomendasi();

  if (phMeter >= 5.5 && phMeter <= 6.0) {
    arr.add("Kentang - pH 5.0-6.0, 90-120 hari"); rekomendasiList[rekomendasiCount++] = "Kentang 90-120h";
    arr.add("Lobak - pH 5.5-6.0, 30-60 hari"); rekomendasiList[rekomendasiCount++] = "Lobak 30-60h";
    arr.add("Wortel - pH 5.5-6.5, 70-80 hari"); rekomendasiList[rekomendasiCount++] = "Wortel 70-80h";
    arr.add("Blueberry - pH 4.5-5.5, 2-3 th"); rekomendasiList[rekomendasiCount++] = "Blueberry 2-3th";
    arr.add("Kangkung - pH 5.3-6.5, 20-30 hari"); rekomendasiList[rekomendasiCount++] = "Kangkung 20-30h";
    arr.add("Cabai  Merah - pH 5.5-6.5, 70-75 hari"); rekomendasiList[rekomendasiCount++] = "Cabai Merah 70-75h";
    arr.add("Terong - pH 5.5-7.5, 70-90 hari"); rekomendasiList[rekomendasiCount++] = "Terong 70-90h";

  } else if (phMeter > 6.0 && phMeter <= 6.5) {
    arr.add("Tomat - pH 6.0-6.8, 75-85 hari"); rekomendasiList[rekomendasiCount++] = "Tomat 75-85h";
    arr.add("Cabai  Merah - pH 5.5-6.5, 70-75 hari"); rekomendasiList[rekomendasiCount++] = "Cabai Merah 70-75h";
    arr.add("Terong - pH 5.5-7.5, 70-90 hari"); rekomendasiList[rekomendasiCount++] = "Terong 70-90h";
    arr.add("Mentimun - pH 6.0-7.0, 50-65 hari"); rekomendasiList[rekomendasiCount++] = "Mentimun 50-65h";
    arr.add("Labu - pH 6.0-7.0, 90-120 hari"); rekomendasiList[rekomendasiCount++] = "Labu 90-120h";
    arr.add("Jagung - pH 6.0-6.8, 75-90 hari"); rekomendasiList[rekomendasiCount++] = "Jagung 75-90h";
    arr.add("Padi - pH 6.0-7.0, 90-120 hari"); rekomendasiList[rekomendasiCount++] = "Padi 90-120h";
    arr.add("Sawi - pH 6.0-7.0, 30-45 hari"); rekomendasiList[rekomendasiCount++] = "Sawi 30-45h";
    arr.add("Kangkung - pH 5.3-6.5, 20-30 hari"); rekomendasiList[rekomendasiCount++] = "Kangkung 20-30h";
    arr.add("Cabai  Rawit - pH 6.0-7.0, 91-122 hari"); rekomendasiList[rekomendasiCount++] = "Cabai Rawit 91-122h";
    arr.add("Bayam - pH 6.0-7.0, 30-45 hari"); rekomendasiList[rekomendasiCount++] = "Bayam 30-45h";

  } else if (phMeter > 6.5 && phMeter <= 7.0) {
    arr.add("Kubis - pH 6.5-7.0, 70-90 hari"); rekomendasiList[rekomendasiCount++] = "Kubis 70-90h";
    arr.add("Brokoli - pH 6.5-7.0, 70-85 hari"); rekomendasiList[rekomendasiCount++] = "Brokoli 70-85h";
    arr.add("Kembang Kol - pH 6.5-7.0, 75-85 hari"); rekomendasiList[rekomendasiCount++] = "K. Kol 75-85h";
    arr.add("Selada - pH 6.5-7.0, 45-65 hari"); rekomendasiList[rekomendasiCount++] = "Selada 45-65h";
    arr.add("Pak Choy - pH 6.5-7.0, 45-60 hari"); rekomendasiList[rekomendasiCount++] = "PakChoy 45-60h";
    arr.add("Padi - pH 6.0-7.0, 90-120 hari"); rekomendasiList[rekomendasiCount++] = "Padi 90-120h";
    arr.add("Sawi - pH 6.0-7.0, 30-45 hari"); rekomendasiList[rekomendasiCount++] = "Sawi 30-45h";
    arr.add("Cabai  Rawit - pH 6.0-7.0, 91-122 hari"); rekomendasiList[rekomendasiCount++] = "Cabai Rawit 91-122h";
    arr.add("Terong - pH 5.5-7.5, 70-90 hari"); rekomendasiList[rekomendasiCount++] = "Terong 70-90h";
    arr.add("Bayam - pH 6.0-7.0, 30-45 hari"); rekomendasiList[rekomendasiCount++] = "Bayam 30-45h";

  } else if (phMeter > 7.0 && phMeter <= 7.5) {
    arr.add("Bawang Merah - pH 6.5-7.5, 65-80 hari"); rekomendasiList[rekomendasiCount++] = "B.Merah 65-80h";
    arr.add("Bawang Putih - pH 6.5-7.5, 90-120 hari"); rekomendasiList[rekomendasiCount++] = "B.Putih 90-120h";
    arr.add("Kacang Panjang - pH 6.8-7.5, 60-75 hari"); rekomendasiList[rekomendasiCount++] = "K.Panjang 60-75h";
    arr.add("Buncis - pH 6.8-7.5, 55-65 hari"); rekomendasiList[rekomendasiCount++] = "Buncis 55-65h";
    arr.add("Kacang Polong - pH 7.0-7.5, 60-70 hari"); rekomendasiList[rekomendasiCount++] = "K.Polong 60-70h";
    arr.add("Terong - pH 5.5-7.5, 70-90 hari"); rekomendasiList[rekomendasiCount++] = "Terong 70-90h";

  } else if (phMeter > 7.5 && phMeter <= 8.0) {
    arr.add("Asparagus - pH 7.0-8.0, 2-3 th"); rekomendasiList[rekomendasiCount++] = "Asparagus 2-3th";
    arr.add("Bit - pH 7.0-8.0, 55-70 hari"); rekomendasiList[rekomendasiCount++] = "Bit 55-70h";
    arr.add("Seledri - pH 6.8-8.0, 85-120 hari"); rekomendasiList[rekomendasiCount++] = "Seledri 85-120h";

  } else {
    arr.add("Tidak ada tanaman sesuai pH");
    rekomendasiList[rekomendasiCount++] = "No Match";
  }
}

// ====== SEND JSON ======
void sendJSON(String json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleStatus() {
  readSensorPH();
  StaticJsonDocument<500> doc;
  doc["ph"] = String(phMeter, 1);
  JsonArray tanaman = doc.createNestedArray("tanaman");
  rekomendasiTanaman(tanaman);

  String out;
  serializeJson(doc, out);
  sendJSON(out);
}

// ====== LCD UPDATE ======
void updateLCD() {
  if (millis() - lastScroll > 3000) {
    lastScroll = millis();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("pH: ");
    lcd.print(phMeter, 1);
    lcd.setCursor(0, 1);
    if (rekomendasiCount > 0) {
      lcd.print(rekomendasiList[currentIndex]);
      currentIndex = (currentIndex + 1) % rekomendasiCount;
    } else {
      lcd.print("No Data");
    }
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PH, INPUT);

  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.print("pH Sensor Ready");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  server.on("/status", handleStatus);
  server.begin();
}

// ====== LOOP ======
void loop() {
  server.handleClient();
  readSensorPH();
  updateLCD();
}
