*
 * Battery Management System - Integrated Project
 * ElevanceSkills Internship - Embedded Systems
 * Karthik Nambiar
 * 
 * Built this over the internship period, started with just
 * reading voltages and kept adding features task by task.
 * Final integrated version combining all 6 tasks.
 */

#define BLYNK_TEMPLATE_ID "TMPL32gNRnrNT"
#define BLYNK_TEMPLATE_NAME "Battery Monitor"
#define BLYNK_AUTH_TOKEN "N7G3yQtzkh_O9opOuvo5JGX81nAOCfNU"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(19, 23, 18, 17, 16, 15);

const int cellPins[4] = {34, 35, 32, 33};
const int relayPin = 25;
const int buzzerPin = 26;

char ssid[] = "Wokwi-GUEST";
char pass[] = "";
bool wifiConnected = false;
bool lastBuzzerState = false;

float cellVoltage[4];
float packTotal;
float packAverage;
float imbalance;
float minV, maxV;
float prevVoltage[4] = {0, 0, 0, 0};
int weakestCell, strongestCell;
const char* healthState;
bool faultActive = false;

int currentScreen = 0;
int lastScreen = -1;
String lastHealthState = "";
float lastPackAverage = 0;

// timing - using millis everywhere, no delay() except wifi reconnect
unsigned long lastReport = 0;
unsigned long lastScreenSwitch = 0;
unsigned long lastBlynkSend = 0;
const unsigned long reportInterval = 1000;
const unsigned long screenInterval = 2000;
const unsigned long blynkInterval = 5000;

String lastSentHealth = "";
float lastSentAverage = 0;

enum SystemMode { NORMAL, DEGRADED, FAILSAFE, SHUTDOWN };
SystemMode currentMode = NORMAL;

bool sensorFault[4] = {false, false, false, false};
int frozenCount[4] = {0, 0, 0, 0};
float frozenPrev[4] = {0, 0, 0, 0};

struct FaultLog {
  String message;
  unsigned long timestamp;
};
FaultLog faultHistory[10];
int faultCount = 0;

void logFault(String message) {
  if (faultCount < 10) {
    faultHistory[faultCount].message = message;
    faultHistory[faultCount].timestamp = millis();
    faultCount++;
    Serial.print("[FAULT] ");
    Serial.print(message);
    Serial.print(" at ");
    Serial.print(millis());
    Serial.println("ms");
  }
}

// task 1 - read all 4 cells
void readCells() {
  packTotal = 0;
  minV = 9999;
  maxV = 0;
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(cellPins[i]);
    cellVoltage[i] = (raw / 4095.0) * 4.2;
    packTotal += cellVoltage[i];
    if (cellVoltage[i] < minV) {
      minV = cellVoltage[i];
      weakestCell = i;
    }
    if (cellVoltage[i] > maxV) {
      maxV = cellVoltage[i];
      strongestCell = i;
    }
  }
}

void calculateStats() {
  packAverage = packTotal / 4.0;
  imbalance = ((maxV - minV) / packAverage) * 100.0;
}

void classifyHealth() {
  if (minV < 2.8) {
    healthState = "PACK FAILURE";
    faultActive = true;
    currentMode = FAILSAFE;
  } else if (imbalance > 20.0) {
    healthState = "CRITICAL IMBALANCE";
    faultActive = true;
    currentMode = FAILSAFE;
  } else if (imbalance > 10.0) {
    healthState = "MINOR IMBALANCE";
    faultActive = false;
  } else {
    healthState = "HEALTHY";
    faultActive = false;
    currentMode = NORMAL;
  }
}

// task 2 - relay and buzzer
// hysteresis: trips at 2.8V, recovers at 3.0V to avoid chatter
void checkRelay() {
  if (minV < 2.8 || maxV > 4.2) {
    digitalWrite(relayPin, LOW);
    digitalWrite(buzzerPin, HIGH);
    if (!lastBuzzerState) {
      if (minV < 2.8) Serial.println("BUZZER: ON (LOW VOLTAGE!)");
      if (maxV > 4.2) Serial.println("BUZZER: ON (OVERVOLTAGE!)");
      lastBuzzerState = true;
    }
  } else if (minV > 3.0 && maxV <= 4.2) {
    digitalWrite(relayPin, HIGH);
    digitalWrite(buzzerPin, LOW);
    if (lastBuzzerState) {
      Serial.println("BUZZER: OFF (Safe)");
      lastBuzzerState = false;
    }
  }
}

void checkFluctuation() {
  for (int i = 0; i < 4; i++) {
    float change = abs(cellVoltage[i] - prevVoltage[i]);
    if (change > 0.5) {
      Serial.print("Cell ");
      Serial.print(i + 1);
      Serial.println(" - High Fluctuation detected!");
    }
    prevVoltage[i] = cellVoltage[i];
  }
}

// task 4 - sensor validation
// note: frozen ADC check doesnt work well in wokwi since pots
// return same value when not moving, disabled for simulation
void checkSensors() {
  int faultedCells = 0;
  for (int i = 0; i < 4; i++) {
    if (cellVoltage[i] <= 0.0 || cellVoltage[i] > 4.19) {
      if (!sensorFault[i]) {
        sensorFault[i] = true;
        logFault("Sensor fault - Cell " + String(i + 1));
      }
    } else {
      sensorFault[i] = false;
      frozenCount[i] = 0;
    }
    frozenPrev[i] = cellVoltage[i];
    if (sensorFault[i]) faultedCells++;
  }

  if (faultedCells == 0 && currentMode != FAILSAFE) currentMode = NORMAL;
  else if (faultedCells == 1) currentMode = DEGRADED;
  else if (faultedCells >= 2) currentMode = FAILSAFE;
}

// task 3 - LCD screens
void showScreen1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Avg:");
  lcd.print(packAverage, 2);
  lcd.print("V");
  lcd.setCursor(0, 1);
  lcd.print(healthState);
}

void showScreen2() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Weak Cell: ");
  lcd.print(weakestCell + 1);
  lcd.setCursor(0, 1);
  lcd.print("Volt: ");
  lcd.print(cellVoltage[weakestCell], 2);
  lcd.print("V");
}

void showScreen3() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Strongest Cell:");
  lcd.print(strongestCell + 1);
  lcd.setCursor(0, 1);
  lcd.print("Volt: ");
  lcd.print(cellVoltage[strongestCell], 2);
  lcd.print("V");
}

void showScreen4() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FAULT DETECTED!");
  lcd.setCursor(0, 1);
  lcd.print(healthState);
}

void updateDisplay() {
  if (faultActive) {
    if (lastScreen != 4) {
      showScreen4();
      lastScreen = 4;
    }
    return;
  }
  unsigned long now = millis();
  if (now - lastScreenSwitch >= screenInterval) {
    lastScreenSwitch = now;
    currentScreen = (currentScreen + 1) % 3;
  }
  // only update LCD when data actually changes, fixes flickering
  if (currentScreen != lastScreen ||
      packAverage != lastPackAverage ||
      String(healthState) != lastHealthState) {
    lastScreen = currentScreen;
    lastPackAverage = packAverage;
    lastHealthState = String(healthState);
    if (currentScreen == 0) showScreen1();
    else if (currentScreen == 1) showScreen2();
    else if (currentScreen == 2) showScreen3();
  }
}

// task 5 - blynk telemetry
String getModeString() {
  if (currentMode == NORMAL) return "NORMAL";
  if (currentMode == DEGRADED) return "DEGRADED";
  if (currentMode == FAILSAFE) return "FAILSAFE";
  return "SHUTDOWN";
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("[WiFi] lost connection, trying to reconnect...");
    WiFi.begin(ssid, pass);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 5000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("\n[WiFi] back online");
    } else {
      Serial.println("\n[WiFi] couldnt reconnect, running offline");
    }
  } else {
    wifiConnected = true;
  }
}

void sendToBlynk() {
  unsigned long now = millis();
  bool intervalReached = (now - lastBlynkSend >= blynkInterval);
  bool stateChanged = (String(healthState) != lastSentHealth ||
                       abs(packAverage - lastSentAverage) > 0.1);

  if (intervalReached || stateChanged) {
    lastBlynkSend = now;
    lastSentHealth = String(healthState);
    lastSentAverage = packAverage;

    // these would work on real hardware with actual blynk connection
    // Blynk.virtualWrite(V0, packAverage);
    // Blynk.virtualWrite(V1, String(healthState));
    // Blynk.virtualWrite(V2, imbalance);
    // Blynk.virtualWrite(V3, getModeString());
    // Blynk.virtualWrite(V4, faultCount);

    // printing to serial since wokwi cant reach blynk servers
    Serial.println("==== BLYNK TELEMETRY ====");
    Serial.print("[BLYNK] V0 Pack Avg    : "); Serial.println(packAverage);
    Serial.print("[BLYNK] V1 Health State: "); Serial.println(healthState);
    Serial.print("[BLYNK] V2 Imbalance   : "); Serial.println(imbalance);
    Serial.print("[BLYNK] V3 Mode        : "); Serial.println(getModeString());
    Serial.print("[BLYNK] V4 Fault Count : "); Serial.println(faultCount);
    Serial.println("=========================");
  }
}

void printReport() {
  Serial.println("=||=||= Battery Report =||=||=");
  for (int i = 0; i < 4; i++) {
    Serial.print("Cell ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(cellVoltage[i], 2);
    Serial.print(" V");
    if (i == weakestCell) Serial.print("  << WEAKEST");
    if (i == strongestCell) Serial.print("  << STRONGEST");
    Serial.println();
  }
  Serial.print("Pack Average : ");
  Serial.print(packAverage, 2);
  Serial.println(" V");
  Serial.print("Imbalance    : ");
  Serial.print(imbalance, 1);
  Serial.println(" %");
  Serial.print("Health State : ");
  Serial.println(healthState);
  Serial.println();
}

void printModeReport() {
  Serial.print("System Mode: ");
  Serial.println(getModeString());
  Serial.println("--- Fault Log ---");
  if (faultCount == 0) {
    Serial.println("No faults recorded");
  } else {
    for (int i = 0; i < faultCount; i++) {
      Serial.print("[");
      Serial.print(i + 1);
      Serial.print("] ");
      Serial.print(faultHistory[i].message);
      Serial.print(" at ");
      Serial.print(faultHistory[i].timestamp);
      Serial.println("ms");
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  lcd.begin(16, 2);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  WiFi.begin(ssid, pass);
  Serial.println("Starting BMS...");
  for (int i = 0; i < 4; i++) {
    prevVoltage[i] = (analogRead(cellPins[i]) / 4095.0) * 4.2;
    frozenPrev[i] = prevVoltage[i];
  }
}

void loop() {
  readCells();
  calculateStats();
  classifyHealth();
  checkRelay();
  checkFluctuation();
  checkSensors();
  checkWiFi();
  sendToBlynk();
  updateDisplay();
  unsigned long now = millis();
  if (now - lastReport >= reportInterval) {
    lastReport = now;
    printReport();
    printModeReport();
  }
}
