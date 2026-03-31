#include <SPI.h>
#include <LoRa.h>

#define LORA_SS    10
#define LORA_RST   5
#define LORA_DIO0  9

int currentSF = 7;
long expectedID = -1;
long receivedCount = 0;
long lostCount = 0;

// ==========================================
// 新增：C++ 函式前置宣告 (Forward Declarations)
// 讓 PlatformIO 的編譯器知道後面有實作這些函式
// ==========================================
void updateParams(int sf);
void resetStats();

void setup() {
  Serial.begin(115200);
  
  // 等待 USB Serial 連線建立 (Teensy 4.0 的 Native USB 特性)
  while (!Serial && millis() < 3000); 

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(915E6)) { 
    Serial.println("LoRa 失敗"); 
    while(1); 
  }
  
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(6);
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0xF1);
  
  // 這裡呼叫 updateParams 時，編譯器因為有前面的宣告，就不會報錯了
  updateParams(7);

  Serial.println("\n--- 接收端指令 ---");
  Serial.println("  p    : 同步至 SF7");
  Serial.println("  7-12 : 同步至目標 SF");
  Serial.println("  r    : 重置統計數據");
}

void loop() {
  if (Serial.available()) {
    String in = Serial.readStringUntil('\n'); 
    in.trim();
    if (in == "p") {
      updateParams(7);
    }
    else if (in == "r") {
      resetStats();
    }

    else { 
      int v = in.toInt(); 
      if (v >= 7 && v <= 12) {
        updateParams(v);
      }

    }
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data = ""; 
    while (LoRa.available()) data += (char)LoRa.read();
    
    float snr = LoRa.packetSnr();
    int rssi = LoRa.packetRssi();

    if (data.startsWith("TST:")) {
      Serial.print("[PRE-TEST] SNR:"); Serial.print(snr); Serial.print(" RSSI:"); Serial.println(rssi);
    } 
    else if (data.startsWith("FRM:") || data.startsWith("STR:")) {
      String tag = data.startsWith("FRM:") ? "[FORM]" : "[STRESS]";

      // 1. 找到冒號的下一個位置 (這就是 ID 的開頭)
      int idStart = data.indexOf(':') + 1;

      // 2. 找到第一個星號的位置 (這就是 ID 的結尾)
      int idEnd = data.indexOf('*');

      // 3. 防呆檢查：如果封包沒有星號 (可能長度設定很短)，結尾就是整個字串的長度
      if (idEnd == -1) {
        idEnd = data.length();
      }

      // 4. 截取出 ID 字串並轉成整數
      String idString = data.substring(idStart, idEnd);
      int id = idString.toInt();
      
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
  
  // ==========================================
  // 修改：標準版 Sandeep Mistry 函式庫不提供此方法，
  // 且在 setSpreadingFactor() 中已經會自動處理 LDO。
  // 因此直接註解或刪除即可。
  // LoRa.setLowDataRateOptimize(sf >= 11); 
  // ==========================================

  resetStats();
  Serial.print(">>> 接收端同步至 SF"); Serial.println(sf);
}

void resetStats() { 
  expectedID = -1; 
  receivedCount = 0; 
  lostCount = 0; 
}
