#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

// ─── OLED Setup ───────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Pin Definitions ──────────────────────────────────
#define TRIG_PIN D6
#define ECHO_PIN D7
#define IR_PIN   D5

// ─── Bin Configuration ────────────────────────────────
#define BIN_HEIGHT_CM 40

// ─── WiFi Credentials ─────────────────────────────────
const char* ssid     = "Shinigami";       // 🔧 change this
const char* password = "your_password";   // 🔧 change this

// ─── ThingSpeak Config ────────────────────────────────
unsigned long channelID = channelId;             // 🔧 your channel ID
const char* writeAPIKey = "your_api_key";  // 🔧 your write API key

// ─── Variables ────────────────────────────────────────
float distanceCM = 0;
int fillPercent = 0;
bool irDetected = false;
unsigned long lastUploadTime = 0;
const int uploadInterval = 15000; // Upload every 15 seconds (ThingSpeak min limit)

WiFiClient client;

// ──────────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 25);
  display.println("Connecting to WiFi");
  display.setCursor(10, 40);
  display.println(ssid);
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Show connected on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("WiFi Connected!");
  display.setCursor(10, 35);
  display.println(WiFi.localIP().toString());
  display.display();
  delay(2000);
}

// ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  // Startup Screen
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("  Waste Monitor");
  display.setCursor(10, 40);
  display.println("  Initializing...");
  display.display();
  delay(2000);

  // Connect WiFi & init ThingSpeak
  connectWiFi();
  ThingSpeak.begin(client);
}

// ──────────────────────────────────────────────────────
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = (duration * 0.0343) / 2.0;

  if (duration == 0) return 0;
  return distance;
}

// ──────────────────────────────────────────────────────
int calculateFillPercent(float distance) {
  if (distance >= BIN_HEIGHT_CM) return 0;
  if (distance <= 0) return 100;

  int percent = (int)(((BIN_HEIGHT_CM - distance) / BIN_HEIGHT_CM) * 100);
  return constrain(percent, 0, 100);
}

// ──────────────────────────────────────────────────────
void uploadToThingSpeak(int fill, float dist, bool ir) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    connectWiFi();
  }

  ThingSpeak.setField(1, fill);           // Field 1 — Fill %
  ThingSpeak.setField(2, dist);           // Field 2 — Distance cm
  ThingSpeak.setField(3, ir ? 1 : 0);    // Field 3 — IR (1=detected, 0=clear)

  int result = ThingSpeak.writeFields(channelID, writeAPIKey);

  if (result == 200) {
    Serial.println("✅ ThingSpeak upload successful!");
  } else {
    Serial.print("❌ Upload failed. Error code: ");
    Serial.println(result);
  }
}

// ──────────────────────────────────────────────────────
void updateOLED(int fill, float dist, bool ir) {
  display.clearDisplay();

  // ── Title ──
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("WASTE MONITOR");

  // ── Divider line ──
  display.drawLine(0, 10, 128, 10, WHITE);

  // ── Fill % (big text) ──
  display.setTextSize(3);
  display.setCursor(25, 18);
  display.print(fill);
  display.print("%");

  // ── Status Bar ──
  display.drawRect(0, 38, 128, 8, WHITE);
  int barWidth = (fill * 126) / 100;
  display.fillRect(1, 39, barWidth, 6, WHITE);

  // ── Distance & IR ──
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Dist: ");
  display.print(dist, 1);
  display.print("cm");

  display.setCursor(80, 50);
  display.print(ir ? "IR: YES" : "IR: NO ");

  display.display();
}

// ──────────────────────────────────────────────────────
void loop() {
  irDetected = (digitalRead(IR_PIN) == LOW);
  distanceCM = measureDistance();
  fillPercent = calculateFillPercent(distanceCM);

  Serial.print("Distance: "); Serial.print(distanceCM);
  Serial.print(" cm | Fill: "); Serial.print(fillPercent);
  Serial.print("% | IR: "); Serial.println(irDetected ? "Detected" : "Clear");

  updateOLED(fillPercent, distanceCM, irDetected);

  // Upload to ThingSpeak every 15 seconds
  if (millis() - lastUploadTime >= uploadInterval) {
    uploadToThingSpeak(fillPercent, distanceCM, irDetected);
    lastUploadTime = millis();
  }

  delay(1000);
}