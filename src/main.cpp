#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "display_bsp.h"   // Waveshare driver (DisplayPort)
#include <Wire.h>
#include <WiFi.h>          // Added: WiFi support
#include "time.h"          // Added: Time handling
#include "esp_sleep.h"
#include "font.h"
#include "orbfont.h"
#include "secfont.h"
#include "PCF85063A-SOLDERED.h"

#define __FREERTOS 1

// --- WiFi Configuration ---
const char* ssid     = "OUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* ntpServer = "pool.ntp.org";   // Global NTP server
const long  gmtOffset_sec = 7200;        // UTC+8 (e.g., Beijing, Singapore)
const int   daylightOffset_sec = 0;       // Daylight savings offset

const int BAT_ADC_PIN = 4; // GPIO4
static const uint8_t SHTC3_ADDR = 0x70;
PCF85063A rtc; 
static const int W = 400;
static const int H = 300;

DisplayPort RlcdPort(12, 11, 5, 40, 41, W, H);
GFXcanvas1 canvas(W, H); // 1-bit sprite/canvas

float t, h;        // temperature and humidity
int sy = 130;      // Graph Y-axis offset
int n = 0;         // Counter for sensor updates
float v_bat = 0;   // Battery voltage

int minn = 0;
int hr = 0;
int sec = 0;

// --- Utility Functions ---

// CRC8 calculation for SHTC3 sensor data validation
uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

// Send command to SHTC3 sensor
bool shtc3_cmd(uint16_t cmd) {
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write((uint8_t)(cmd >> 8));
  Wire.write((uint8_t)(cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

// Read temperature and humidity from SHTC3
bool shtc3_read(float &tempC, float &rh) {
  // Wakeup sensor
  if (!shtc3_cmd(0x3517)) return false;
  delay(1);

  // Measure (normal power, clock stretching disabled)
  if (!shtc3_cmd(0x7866)) return false;
  delay(20);

  Wire.requestFrom((int)SHTC3_ADDR, 6);
  if (Wire.available() != 6) return false;

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  // Validate data with CRC
  if (crc8(&d[0], 2) != d[2]) return false;
  if (crc8(&d[3], 2) != d[5]) return false;

  uint16_t tRaw  = (uint16_t)d[0] << 8 | d[1];
  uint16_t rhRaw = (uint16_t)d[3] << 8 | d[4];

  // Convert raw values to physical units
  tempC = -45.0f + 175.0f * (float)tRaw / 65535.0f;
  rh    = 100.0f * (float)rhRaw / 65535.0f;

  // Put sensor to sleep
  shtc3_cmd(0xB098);
  return true;
}

// --- WiFi NTP Time Sync ---
void syncTimeWithNTP() {
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  
  // Wait for connection (timeout after ~10 seconds)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! Fetching NTP time...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // Write NTP time to the hardware RTC
      rtc.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      // setDate format: (weekday, day, month, year)
      rtc.setDate(timeinfo.tm_wday, timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      Serial.println("Hardware RTC updated successfully!");
    }
    
    // Disconnect WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi turned off.");
  } else {
    Serial.println("\nWiFi connection failed. Proceeding with existing RTC time.");
  }
}

// Push 1-bit GFX canvas to the RLCD display
void pushCanvasToRLCD(bool invert = false) {
  uint8_t *buf = canvas.getBuffer(); 
  const int bytesPerRow = (W + 7) / 8; // 50 bytes for 400px

  RlcdPort.RLCD_ColorClear(ColorWhite);

  for (int y = 0; y < H; y++) {
    uint8_t *row = buf + y * bytesPerRow;
    for (int bx = 0; bx < bytesPerRow; bx++) {
      uint8_t v = row[bx];
      if (invert) v ^= 0xFF;

      int x0 = bx * 8;
      for (int bit = 0; bit < 8; bit++) {
        int x = x0 + bit;
        if (x >= W) break;

        bool on = (v & (0x80 >> bit)) != 0;
        if (on) {
          RlcdPort.RLCD_SetPixel((uint16_t)x, (uint16_t)y, ColorBlack);
        }
      }
    }
  }

  RlcdPort.RLCD_Display();
}

// Map battery voltage to UI segments
int batteryToSegments(float vbat) {
  if (vbat >= 4.0)  return 5;
  if (vbat >= 3.90) return 4;
  if (vbat >= 3.80) return 3;
  if (vbat >= 3.65) return 2;
  if (vbat >= 3.50) return 1;
  return 0;
}

void draw() {
  canvas.fillScreen(0);
  
  // Outer Frame
  canvas.fillRect(10, 10, 5, 280, 1);
  canvas.fillRect(385, 10, 5, 280, 1);
  canvas.fillRect(10, 10, 380, 5, 1);
  canvas.fillRect(10, 285, 380, 5, 1);

  // Battery Frame
  canvas.fillRect(308, 24, 60, 28, 1);
  canvas.fillRect(312, 28, 52, 20, 0);
  canvas.fillRect(368, 32, 5, 12, 1);

  // Battery Segments
  for(int i=0; i<batteryToSegments(v_bat); i++) {
    canvas.fillRect(314+(i*10), 30, 7, 16, 1);
  }

  // Seconds Graph
  for(int j=0; j<60; j++) {
    for(int i=0; i<10; i++) {
      if(j!=12 && j!=24 && j!=36 && j!=48) {
        int xPos = 20+(j*6);
        int yPos = (j<20) ? (sy-(j*3)+(i*5)) : (sy-(20*3)+(i*5));
        
        if(j <= sec) canvas.fillRect(xPos, yPos, 4, 4, 1); // Filled if second has passed
        else         canvas.drawRect(xPos, yPos, 4, 4, 1); // Outline if second is pending
      }
    }
  }

  // UI Decorations
  canvas.fillRect(90, 165, 290, 4, 1); // Horizontal line under graph

  // Temperature Label
  canvas.fillRoundRect(162, 131, 70, 28, 6, 1);
  canvas.setTextColor(0);
  canvas.setFont(&Orbitron_Medium_22);
  canvas.setCursor(170, 153);
  canvas.print("TMP");

  // Temperature Value
  canvas.setTextColor(1);
  canvas.setFont(&Orbitron_Medium_38);
  canvas.setCursor(240, 158);
  canvas.print(t);
 
  // Main Time (HH:MM)
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hr, minn);
  canvas.setFont(&DSEG7_Classic_Bold_84);
  canvas.setCursor(20, 270);
  canvas.print(timeStr);

  // Seconds (SS)
  char secStr[4];
  sprintf(secStr, "%02d", sec);
  canvas.setFont(&DSEG7_Classic_Bold_36);
  canvas.setCursor(320, 224);
  canvas.print(secStr);

  // Branding/Tags
  canvas.fillRoundRect(25, 25, 140, 30, 4, 1);
  canvas.setTextColor(0);
  canvas.setFont(&Orbitron_Medium_22);
  canvas.setCursor(35, 48);
  canvas.print("RLCD");

  canvas.fillRect(170, 25, 80, 3, 1);
  canvas.setTextColor(1);
  canvas.setFont(&Orbitron_Medium_19);
  canvas.setCursor(170, 48);
  canvas.print("ESP32 S3");

  canvas.setFont(&Orbitron_Medium_15);
  canvas.setCursor(25, 72);
  canvas.print("VOLOS");
    
  pushCanvasToRLCD(false);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Battery ADC configuration
  analogReadResolution(12); // 0..4095
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);

  Wire.begin(13, 14);
  RlcdPort.RLCD_Init();
  
  rtc.begin();
  
  // --- Synchronize time on boot ---
  syncTimeWithNTP();

  // Enable wake-up every 10 minutes (if using deep sleep)
  esp_sleep_enable_timer_wakeup(10ULL * 6000000ULL);
}

void loop() {
  // Get current time from hardware RTC
  minn = rtc.getMinute();
  hr   = rtc.getHour();
  sec  = rtc.getSecond();
 
  // Update battery and sensors every 200 cycles
  if(n == 0) { 
    int raw = analogRead(BAT_ADC_PIN);
    float v_adc = (raw / 4095.0f) * 3.3f;   
    v_bat = v_adc * 3.0f * 1.079; // Voltage divider compensation

    Serial.printf("raw=%d  Vadc=%.3f V  Vbat=%.3f V\n", raw, v_adc, v_bat);
    
    if (shtc3_read(t, h)) {
      Serial.printf("T = %.2f C, RH = %.2f %%\n", t, h);
    } else {
      Serial.println("SHTC3 read failed");
    }
  }

  n++;
  if(n >= 200) n = 0;

  draw();
  
  // Optional: esp_light_sleep_start(); 
}
