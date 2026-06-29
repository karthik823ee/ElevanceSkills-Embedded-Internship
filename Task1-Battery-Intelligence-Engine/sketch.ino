const int cellPins[4] = {34, 35, 32, 33};
float cellVoltage[4];
float packTotal;
float packAverage;
float imbalance;
float minV, maxV;
int weakestCell, strongestCell;
const char* healthState;

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

void setup() {
  Serial.begin(9600);
}

void loop() {
  readCells();
  calculateStats();
  classifyHealth();
  printReport();
  delay(1000);
}
