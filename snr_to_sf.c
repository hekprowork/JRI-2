#include <stdio.h>

/**
 * LoRa SNR to Spreading Factor (SF) Calculator
 * Based on Semtech RFM95 Technical Specifications
 */

void suggest_sf(float snr) {
    printf("\n--- 分析結果 ---\n");
    printf("當前測得 SNR: %.2f dB\n", snr);

    if (snr >= -7.5) {
        printf("建議擴頻因子: SF 7\n");
        printf("技術備註: 訊號良好，可使用最小 SF 以獲得最高數據傳輸率。\n");
        printf("建議最小發送間隔: 100ms\n");
    } else if (snr >= -10.0) {
        printf("建議擴頻因子: SF 8\n");
        printf("建議最小發送間隔: 200ms\n");
    } else if (snr >= -12.5) {
        printf("建議擴頻因子: SF 9\n");
        printf("建議最小發送間隔: 400ms\n");
    } else if (snr >= -15.0) {
        printf("建議擴頻因子: SF 10\n");
        printf("建議最小發送間隔: 800ms\n");
    } else if (snr >= -17.5) {
        printf("建議擴頻因子: SF 11\n");
        printf("建議最小發送間隔: 1600ms\n");
    } else if (snr >= -20.0) {
        printf("建議擴頻因子: SF 12\n");
        printf("技術備註: 訊號極弱，需使用最大擴頻因子。\n");
        printf("建議最小發送間隔: 3200ms\n");
    } else {
        printf("建議狀態: 無法通訊 (Out of Range)\n");
        printf("技術備註: 當前 SNR (%.2f dB) 低於 SF12 的解調極限 (-20 dB)。\n", snr);
    }
    printf("----------------\n");
}

int main() {
    float user_snr;

    printf("========================================\n");
    printf("   LoRa 通訊參數建議工具 (RFM95 基準)   \n");
    printf("========================================\n");
    printf("請輸入接收端測得的平均 SNR (dB): ");

    if (scanf("%f", &user_snr) != 1) {
        printf("輸入錯誤，請請輸入數字。\n");
        return 1;
    }

    suggest_sf(user_snr);

    return 0;
}
