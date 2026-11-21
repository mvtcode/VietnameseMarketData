# VietnameseMarketData

Thư viện ESP32 / Arduino cập nhật **Giá vàng** (SJC, Nhẫn 9999, Thế giới) và **Giá xăng dầu** (RON 95, E5 RON 92, Dầu Diesel, Dầu Mazut) tại thị trường Việt Nam.

---

## ✨ Tính năng nổi bật

- 🟡 **Giá vàng**: Cập nhật giá Mua vào / Bán ra của Vàng miếng SJC, Vàng nhẫn trơn 9999 và Vàng thế giới.
- ⛽ **Giá xăng dầu**: Giá bán lẻ và mức tăng/giảm (+/-) của Xăng RON 95-III, E5 RON 92, Dầu Diesel 0.05S, Dầu Mazut.
- 🛡️ **Tự động Fallback HTTP / HTTPS**: Tự động kiểm tra heap an toàn trước khi handshake SSL/TLS; tự động fallback qua HTTP port 80 nếu thiếu RAM hoặc mạng chặn SSL, ngăn chặn 100% crash OOM trên ESP32.
- ⚡ **Non-blocking FreeRTOS Task**: Hỗ trợ fetch dữ liệu ngầm bằng FreeRTOS task, không gây giật lag màn hình UI/LVGL.
- ⏱️ **Tùy chỉnh chu kỳ**: Dễ dàng cấu hình khoảng thời gian tự động đồng bộ (mặc định 30 phút).

---

## 📦 Cài đặt

Thêm vào `platformio.ini`:
```ini
lib_deps =
    https://github.com/mvtcode/VietnameseMarketData.git
```

Hoặc gắn tag phiên bản:
```ini
lib_deps =
    https://github.com/mvtcode/VietnameseMarketData.git#v1.0.0
```

---

## 🚀 Hướng dẫn sử dụng nhanh

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <VietnameseMarketData.h>

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    VietnameseMarketData::init();
    VietnameseMarketData::setUpdateInterval(30); // 30 phut
}

void loop() {
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    VietnameseMarketData::update(isConnected);

    MarketInfo market = VietnameseMarketData::getMarket();
    if (market.is_valid) {
        Serial.printf("SJC Mua: %s | Ban: %s\n", market.sjc_buy_str, market.sjc_sell_str);
        Serial.printf("Nhan Mua: %s | Ban: %s\n", market.ring_buy_str, market.ring_sell_str);
        Serial.printf("Xang RON95: %d VND (Chenh lech: %d)\n", market.ron95_price, market.ron95_delta);
        Serial.printf("Xang E5: %d VND | Dau Diesel: %d VND\n", market.e5_price, market.diesel_price);
    }

    delay(5000);
}
```

---

## 📄 License
MIT License - Developed by [mvtcode](https://github.com/mvtcode).
