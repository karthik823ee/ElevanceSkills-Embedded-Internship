const int cellPins[4] = {34, 35, 32, 33};
const int relayPin = 25;
const int buzzerPin = 26;

float cellVoltage[4];
float packTotal;
float packAverage;
float imbalance;
float minV, maxV;
int weakestCell, strongestCell;
const char* healthState;

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

void checkRelay() {
  if (minV < 2.8) {
    digitalWrite(relayPin, LOW);
    digitalWrite(buzzerPin, HIGH);
    Serial.println("BUZZER: ON (LOW VOLTAGE!)");
  } else if (minV > 3.0) {
    digitalWrite(relayPin, HIGH);
    digitalWrite(buzzerPin, LOW);
    Serial.println("BUZZER: OFF (Safe)");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  readCells();
  calculateStats();
  classifyHealth();
  checkRelay();

  unsigned long now = millis();
  if (now - lastReport >= reportInterval) {
    lastReport = now;
    printReport();
  }
}
