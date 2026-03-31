#include <SPI.h>
#include <LoRa.h>

#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define NSS_PIN 33
#define RST_PIN 26
#define DIO0_PIN 13

enum Mode { IDLE, PRE_TEST, FORMAL_TEST, STRESS_TEST };
Mode currentMode = IDLE;
int currentSF = 7;
unsigned long testInterval = 1000; // [修復] 改為 unsigned long，避免與 millis() 計算時發生型態問題
unsigned long packetCounter = 0;
unsigned long lastSendTime = 0;
const int packetLimit = 100;
int targetPayloadLength = 32; // 新增：預設封包長度為 32 Bytes

void handleSerial();
void sendPacket();
void updateParams(int sf);
void printMenu();
unsigned long getSafeInterval(int sf); // 回傳型態配合 testInterval 更改

void setup() {
  Serial.begin(115200);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, NSS_PIN);
  LoRa.setPins(NSS_PIN , RST_PIN, DIO0_PIN);
  
  if (!LoRa.begin(915E6)) { 
    Serial.println("LoRa 初始化失敗"); 
    while(1); 
  }
  
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(6);
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0xF1);
  printMenu();
}

void loop() {
  handleSerial();

  if (currentMode != IDLE) {
    // 檢查是否達到發送間隔
    if (millis() - lastSendTime > testInterval) {
      
      sendPacket();
      
      // [修復] 將時間更新放在 sendPacket 之後！
      // 確保 testInterval 是「封包發射完畢」到「下一次開始」的真實冷卻時間
      lastSendTime = millis(); 

      // 檢查是否達到任務上限
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
    String lowerInput = input;
    lowerInput.toLowerCase(); // 統一轉小寫方便判斷
    
    if (lowerInput == "p") {
      currentMode = PRE_TEST;
      testInterval = 1000;
      updateParams(7);
      Serial.println("\n>>> [環境預測量階段] SF7, Interval 1s");
      lastSendTime = millis() - testInterval; // 讓第一個封包立刻觸發
      
    } else if (lowerInput == "x") {
      currentMode = IDLE;
      Serial.println("\n>>> 已停止。");
      
    } else if (lowerInput.startsWith("s ")) {
      // [修復] 使用 sscanf 解析字串，防呆設計，避免使用者輸入錯誤格式導致崩潰
      int sf = 0, inter = 0;
      if (sscanf(input.c_str(), "s %d %d", &sf, &inter) == 2 || 
          sscanf(input.c_str(), "S %d %d", &sf, &inter) == 2) {
            
        if (sf >= 7 && sf <= 12 && inter > 0) {
          currentMode = STRESS_TEST;
          testInterval = inter;
          updateParams(sf);
          Serial.print("\n>>> [極限壓力測試] SF"); Serial.print(sf);
          Serial.print(", Interval "); Serial.print(inter); Serial.println("ms");
          lastSendTime = millis() - testInterval; // 立刻觸發
        } else {
          Serial.println(">>> 參數錯誤：SF 需介於 7-12，Interval 需 > 0");
        }
      } else {
        Serial.println(">>> 格式錯誤：請使用 s [SF] [Interval] (例如 s 7 150)");
      }
      
    } else if (lowerInput.startsWith("l ")) {
      // 新增：解析設定封包長度的指令 (例如 l 64)
      int len = 0;
      if (sscanf(input.c_str(), "l %d", &len) == 1 || 
          sscanf(input.c_str(), "L %d", &len) == 1) {
            
        if (len >= 10 && len <= 255) { // 限制合理範圍，避免當機或超過硬體極限
          targetPayloadLength = len;
          Serial.print("\n>>> [設定] 目標封包長度已更改為: "); 
          Serial.print(targetPayloadLength); Serial.println(" Bytes");
        } else {
          Serial.println(">>> 參數錯誤：長度需介於 10 到 255 Bytes 之間");
        }
      } else {
        Serial.println(">>> 格式錯誤：請使用 l [長度] (例如 l 64)");
      }
    } else {
      int sf = input.toInt();
      if (sf >= 7 && sf <= 12) {
        currentMode = FORMAL_TEST;
        updateParams(sf);
        testInterval = getSafeInterval(sf);
        Serial.print("\n>>> [正式測試階段] SF"); Serial.print(sf);
        Serial.print(", Interval "); Serial.print(testInterval); Serial.println("ms");
        lastSendTime = millis() - testInterval; // 立刻觸發
      }
    }
  }
}

void updateParams(int sf) {
  currentSF = sf;
  packetCounter = 0;
  LoRa.setSpreadingFactor(sf);
}

void sendPacket() {
  unsigned long start = millis();
  
  // 1. 組裝基礎訊息 (為了節省空間，稍微縮寫 Header)
  String payload = "";
  if (currentMode == PRE_TEST) payload += "TST:";
  else if (currentMode == STRESS_TEST) payload += "STR:";
  else payload += "FRM:";
  
  payload += String(packetCounter);

  // 2. 自動填充 (Padding) 直到滿足使用者設定的長度
  // 注意：如果使用者設定的長度比基礎訊息(例如 TST:0)還短，這個迴圈就不會執行，直接送出基礎訊息
  while (payload.length() < targetPayloadLength) {
    payload += "*"; 
  }

  // 3. 發射封包
  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket(); 
  unsigned long duration = millis() - start;

  // 4. 印出結果
  Serial.print(currentMode == PRE_TEST ? "[PRE]" : (currentMode == STRESS_TEST ? "[STRESS]" : "[FORM]"));
  Serial.print(" SF"); Serial.print(currentSF);
  Serial.print(" | ID:"); Serial.print(packetCounter);
  Serial.print(" | Len:"); Serial.print(payload.length()); // 顯示實際送出的 Bytes
  Serial.print(" | ToA:"); Serial.print(duration); Serial.println("ms");
  
  packetCounter++;
}

unsigned long getSafeInterval(int sf) {
  if (sf <= 7) return 250;
  if (sf == 8) return 500;
  if (sf == 9) return 1000;
  if (sf == 10) return 2000;
  return 5000; // SF11, SF12 給予較長的保護間隔
}

void printMenu() {
  Serial.println("\n--- 控制指令 ---");
  Serial.println("  l [Len]    : 設定封包長度 (例如 l 64，預設 32)"); // 新增這行
  Serial.println("  p          : 環境測試(SF7)");
  Serial.println("  7-12       : 正式測試(100包)");
  Serial.println("  s [SF] [I] : 壓力測試(s 7 150)");
  Serial.println("  x          : 停止");
}
