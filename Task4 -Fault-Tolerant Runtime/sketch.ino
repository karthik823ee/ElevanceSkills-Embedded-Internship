#include <LiquidCrystal.h>
LiquidCrystal lcd(19, 23, 18, 17, 16, 15);

const int cellPins[4] = {34, 35, 32, 33};
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
unsigned long lastReport = 0;
unsigned long lastScreenSwitch = 0;
const unsigned long reportInterval = 1000;
const unsigned long screenInterval = 2000;
int lastScreen = -1;
String lastHealthState = "";
float lastPackAverage = 0;

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
  }
}
void checkSensors() {
  int faultedCells = 0;
  for (int i = 0; i < 4; i++) {

    // Check 1 — Disconnection only (frozen ADC disabled in simulation)
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

  if (faultedCells == 0) currentMode = NORMAL;
  else if (faultedCells == 1) currentMode = DEGRADED;
  else if (faultedCells >= 2) currentMode = FAILSAFE;
}
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
  if (currentMode == NORMAL) Serial.println("NORMAL");
  else if (currentMode == DEGRADED) Serial.println("DEGRADED");
  else if (currentMode == FAILSAFE) Serial.println("FAILSAFE");
  else if (currentMode == SHUTDOWN) Serial.println("SHUTDOWN");
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
  Serial.begin(9600);
  lcd.begin(16, 2);
  for (int i = 0; i < 4; i++) {
    prevVoltage[i] = (analogRead(cellPins[i]) / 4095.0) * 4.2;
    frozenPrev[i] = prevVoltage[i];
  }
}
void loop() {
  readCells();
  calculateStats();
  classifyHealth();
  checkSensors();
  updateDisplay();
  unsigned long now = millis();
  if (now - lastReport >= reportInterval) {
    lastReport = now;
    printReport();
    printModeReport();
  }
}
