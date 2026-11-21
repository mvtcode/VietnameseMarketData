#include <Arduino.h>
#include <WiFi.h>
#include <VietnameseMarketData.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Dang ket noi WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    VietnameseMarketData::init();

    // Thu fetch truc tiep (blocking) hoac dung VietnameseMarketData::update() non-blocking
    Serial.println("Dang tai du lieu thi truong Viet Nam...");
    if (VietnameseMarketData::fetchNow()) {
        MarketInfo market = VietnameseMarketData::getMarket();
        Serial.println("=== THI TRUONG VIET NAM ===");
        Serial.printf("Vang SJC: Mua %s - Ban %s\n", market.sjc_buy_str, market.sjc_sell_str);
        Serial.printf("Vang Nhan 9999: Mua %s - Ban %s\n", market.ring_buy_str, market.ring_sell_str);
        Serial.printf("Vang The Gioi: Mua %s - Ban %s\n", market.world_buy_str, market.world_sell_str);
        Serial.printf("Xang RON 95-III: %d VND (Diff: %d)\n", market.ron95_price, market.ron95_delta);
        Serial.printf("Xang E5 RON 92: %d VND (Diff: %d)\n", market.e5_price, market.ron92_delta);
        Serial.printf("Dau Diesel 0.05S: %d VND (Diff: %d)\n", market.diesel_price, market.diesel_delta);
        Serial.printf("Dau Mazut: %d VND (Diff: %d)\n", market.mazut_price, market.mazut_delta);
    } else {
        Serial.println("Lay du lieu that bai!");
    }
}

void loop() {
    // Non-blocking update dinh ky trong loop
    VietnameseMarketData::update(WiFi.status() == WL_CONNECTED);
    delay(1000);
}
