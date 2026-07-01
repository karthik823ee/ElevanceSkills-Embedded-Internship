const int cellPins[4] = {34, 35, 32, 33};
const int relayPin = 25;
const int buzzerPin = 26;

float cellVoltage[4];
float packTotal;
float packAverage;
float imbalance;
float minV, maxV;
float prevVoltage[4] = {0, 0, 0, 0};
int weakestCell, strongestCell;
const char* healthState;
bool lastBuzzerState = false;;

unsigned long lastReport = 0;
const unsigned long reportInterval = 1000;

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
  } else if (imbalance > 20.0) {
    healthState = "CRITICAL IMBALANCE";
  } else if (imbalance > 10.0) {
    healthState = "MINOR IMBALANCE";
  } else {
    healthState = "HEALTHY";
  }
}

void checkRelay() {
  if (minV < 2.8 || maxV > 4.2) {
    digitalWrite(relayPin, LOW);
    digitalWrite(buzzerPin, HIGH);
    if (!lastBuzzerState) {
      if (minV < 2.8) Serial.println("BUZZER: ON (LOW VOLTAGE!)");
      if (maxV > 4.2) Serial.println("BUZZER: ON (OVERVOLTAGE!)");
      lastBuzzerState = true;
    }
  } else if (minV > 3.0&& maxV <= 4.2) {
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
      Serial.println(" in Danger - High Fluctuation!");
    }
    prevVoltage[i] = cellVoltage[i];
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

void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  for (int i = 0; i < 4; i++) {
    prevVoltage[i] = (analogRead(cellPins[i]) / 4095.0) * 4.2;
  }
}

void loop() {
  readCells();
  calculateStats();
  classifyHealth();
  checkRelay();
  checkFluctuation();
  unsigned long now = millis();
  if (now - lastReport >= reportInterval) {
    lastReport = now;
    printReport();
  }
}
