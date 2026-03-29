#include <SPI.h>
#include <LoRa.h>

#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

int currentSF = 7;
long expectedID = -1;
long receivedCount = 0;
long lostCount = 0;

void setup() {
  Serial.begin(115200);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) { Serial.println("LoRa 失敗"); while(1); }
  
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(6);
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0xF1);
  updateParams(7);

  Serial.println("\n--- 接收端指令 ---");
  Serial.println("  p    : 同步至 SF7");
  Serial.println("  7-12 : 同步至目標 SF");
  Serial.println("  r    : 重置統計數據");
}

void loop() {
  if (Serial.available()) {
    String in = Serial.readStringUntil('\n'); in.trim();
    if (in == "p") updateParams(7);
    else if (in == "r") resetStats();
    else { int v = in.toInt(); if (v>=7 && v<=12) updateParams(v); }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data = ""; while (LoRa.available()) data += (char)LoRa.read();
    float snr = LoRa.packetSnr();
    int rssi = LoRa.packetRssi();

    if (data.startsWith("TEST:")) {
      Serial.print("[PRE-TEST] SNR:"); Serial.print(snr); Serial.print(" RSSI:"); Serial.println(rssi);
    } 
    else if (data.startsWith("FORMAL:") || data.startsWith("STRESS:")) {
      String tag = data.startsWith("FORMAL:") ? "[FORM]" : "[STRESS]";
      int offset = data.startsWith("FORMAL:") ? 7 : 7; // Both are length 7 or adjust accordingly
      if (data.startsWith("STRESS:")) offset = 7; 
      
      long id = data.substring(offset).toInt();
      receivedCount++;
      if (expectedID != -1 && id > expectedID) lostCount += (id - expectedID);
      expectedID = id + 1;
      float loss = (float)lostCount / (receivedCount + lostCount) * 100.0;

      Serial.print(tag); Serial.print(" ID:"); Serial.print(id);
      Serial.print(" | SNR:"); Serial.print(snr);
      Serial.print(" | RSSI:"); Serial.print(rssi);
      Serial.print(" | Loss:"); Serial.print(loss); Serial.println("%");
    }
  }
}

void updateParams(int sf) {
  currentSF = sf;
  LoRa.setSpreadingFactor(sf);
  LoRa.setLowDataRateOptimize(sf >= 11);
  resetStats();
  Serial.print(">>> 接收端同步至 SF"); Serial.println(sf);
}

void resetStats() { expectedID = -1; receivedCount = 0; lostCount = 0; }
