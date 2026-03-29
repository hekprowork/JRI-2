#include <SPI.h>
#include <LoRa.h>

#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

enum Mode { IDLE, PRE_TEST, FORMAL_TEST, STRESS_TEST };
Mode currentMode = IDLE;
int currentSF = 7;
int testInterval = 1000;
unsigned long packetCounter = 0;
unsigned long lastSendTime = 0;
const int packetLimit = 100;

void handleSerial();
void sendPacket();
void updateParams(int sf);
void printMenu();

void setup() {
  Serial.begin(115200);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) { Serial.println("LoRa 失敗"); while(1); }
  
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(6);
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0xF1);
  printMenu();
}

void loop() {
  handleSerial();

  if (currentMode != IDLE) {
    if (millis() - lastSendTime > testInterval) {
      lastSendTime = millis();
      sendPacket();

      if ((currentMode == FORMAL_TEST || currentMode == STRESS_TEST) && packetCounter >= packetLimit) {
        Serial.println("\n>>> 測試任務完成，回到待機。");
        currentMode = IDLE;
        printMenu();
      }
    }
  }
}

void handleSerial() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.equalsIgnoreCase("p")) {
      currentMode = PRE_TEST;
      testInterval = 1000;
      updateParams(7);
      Serial.println("\n>>> [環境預測量階段] SF7, Interval 1s");
    } else if (input.equalsIgnoreCase("x")) {
      currentMode = IDLE;
      Serial.println("\n>>> 已停止。");
    } else if (input.startsWith("s ")) { // 壓力測試範例: s 7 150
      int firstSpace = input.indexOf(' ');
      int lastSpace = input.lastIndexOf(' ');
      int sf = input.substring(firstSpace + 1, lastSpace).toInt();
      int inter = input.substring(lastSpace + 1).toInt();
      if (sf >= 7 && sf <= 12 && inter > 0) {
        currentMode = STRESS_TEST;
        testInterval = inter;
        updateParams(sf);
        Serial.print("\n>>> [極限壓力測試] SF"); Serial.print(sf);
        Serial.print(", Interval "); Serial.print(inter); Serial.println("ms");
      }
    } else {
      int sf = input.toInt();
      if (sf >= 7 && sf <= 12) {
        currentMode = FORMAL_TEST;
        updateParams(sf);
        testInterval = getSafeInterval(sf);
        Serial.print("\n>>> [正式測試階段] SF"); Serial.print(sf);
        Serial.print(", Interval "); Serial.print(testInterval); Serial.println("ms");
      }
    }
  }
}

void updateParams(int sf) {
  currentSF = sf;
  packetCounter = 0;
  LoRa.setSpreadingFactor(sf);
  LoRa.setLowDataRateOptimize(sf >= 11);
}

void sendPacket() {
  unsigned long start = millis();
  LoRa.beginPacket();
  if (currentMode == PRE_TEST) LoRa.print("TEST:");
  else if (currentMode == STRESS_TEST) LoRa.print("STRESS:");
  else LoRa.print("FORMAL:");
  LoRa.print(packetCounter);
  LoRa.endPacket();
  unsigned long duration = millis() - start;

  Serial.print(currentMode == PRE_TEST ? "[PRE]" : (currentMode == STRESS_TEST ? "[STRESS]" : "[FORM]"));
  Serial.print(" SF"); Serial.print(currentSF);
  Serial.print(" | ID:"); Serial.print(packetCounter);
  Serial.print(" | ToA:"); Serial.print(duration); Serial.println("ms");
  packetCounter++;
}

int getSafeInterval(int sf) {
  if (sf <= 7) return 250;
  if (sf == 8) return 500;
  if (sf == 9) return 1000;
  if (sf == 10) return 2000;
  return 5000;
}

void printMenu() {
  Serial.println("\n--- 控制指令 ---");
  Serial.println("  p          : 環境測試(SF7)");
  Serial.println("  7-12       : 正式測試(100包)");
  Serial.println("  s [SF] [I] : 壓力測試(s 7 150)");
  Serial.println("  x          : 停止");
}
