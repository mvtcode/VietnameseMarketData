#include "VietnameseMarketData.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#if defined(DEBUG_VIETNAMESE_MARKET) || defined(ENABLE_SERIAL_LOG)
#define VMD_LOG_I(fmt, ...) Serial.printf("[Market][I] " fmt "\n", ##__VA_ARGS__)
#define VMD_LOG_W(fmt, ...) Serial.printf("[Market][W] " fmt "\n", ##__VA_ARGS__)
#define VMD_LOG_E(fmt, ...) Serial.printf("[Market][E] " fmt "\n", ##__VA_ARGS__)
#else
#define VMD_LOG_I(...)
#define VMD_LOG_W(...)
#define VMD_LOG_E(...)
#endif

MarketInfo VietnameseMarketData::current_market;
SemaphoreHandle_t VietnameseMarketData::marketMutex = NULL;
uint32_t VietnameseMarketData::last_fetch_time = 0;
bool VietnameseMarketData::is_fetching = false;
uint32_t VietnameseMarketData::update_interval_ms = 30 * 60 * 1000; // 30 phut

static String extractJsonStr(const String &str, int startPos, const char *key) {
    int keyIdx = str.indexOf(key, startPos);
    if (keyIdx == -1) return "";
    int valStart = keyIdx + strlen(key);
    while (valStart < (int)str.length() && (str[valStart] == ' ' || str[valStart] == ':')) {
        valStart++;
    }
    if (valStart < (int)str.length() && str[valStart] == '"') {
        valStart++;
        int valEnd = str.indexOf('"', valStart);
        if (valEnd == -1) return "";
        return str.substring(valStart, valEnd);
    }
    int valEnd = valStart;
    while (valEnd < (int)str.length() && str[valEnd] != ',' && str[valEnd] != '}') {
        valEnd++;
    }
    String res = str.substring(valStart, valEnd);
    res.trim();
    return res;
}

static int extractJsonInt(const String &str, int startPos, const char *key) {
    int keyIdx = str.indexOf(key, startPos);
    if (keyIdx == -1) return 0;
    int valStart = keyIdx + strlen(key);
    while (valStart < (int)str.length() && (str[valStart] == ' ' || str[valStart] == '"' || str[valStart] == ':')) {
        valStart++;
    }
    int valEnd = valStart;
    while (valEnd < (int)str.length() && (isdigit(str[valEnd]) || str[valEnd] == '-' || str[valEnd] == '.')) {
        valEnd++;
    }
    String numStr = str.substring(valStart, valEnd);
    numStr.replace(".", "");
    numStr.replace(",", "");
    return numStr.toInt();
}

static bool parseMarketPayload(const String &payload, MarketInfo &outInfo) {
    if (payload.length() == 0) return false;
    bool parsed = false;

    // 1. Gia Vang SJC
    int sjcIdx = payload.indexOf("\"sjc\":");
    if (sjcIdx != -1) {
        String bStr = extractJsonStr(payload, sjcIdx, "\"buy\":");
        String sStr = extractJsonStr(payload, sjcIdx, "\"sell\":");
        if (bStr.length() > 0) {
            strncpy(outInfo.sjc_buy_str, bStr.c_str(), sizeof(outInfo.sjc_buy_str) - 1);
            parsed = true;
        }
        if (sStr.length() > 0) {
            strncpy(outInfo.sjc_sell_str, sStr.c_str(), sizeof(outInfo.sjc_sell_str) - 1);
            parsed = true;
        }
    }

    // 2. Gia Vang Nhan 9999
    int ringIdx = payload.indexOf("\"vangnhan9999\":");
    if (ringIdx != -1) {
        String rbStr = extractJsonStr(payload, ringIdx, "\"buy\":");
        String rsStr = extractJsonStr(payload, ringIdx, "\"sell\":");
        if (rbStr.length() > 0) strncpy(outInfo.ring_buy_str, rbStr.c_str(), sizeof(outInfo.ring_buy_str) - 1);
        if (rsStr.length() > 0) strncpy(outInfo.ring_sell_str, rsStr.c_str(), sizeof(outInfo.ring_sell_str) - 1);
    }

    // 3. Gia Vang The Gioi
    int worldIdx = payload.indexOf("\"thegioi\":");
    if (worldIdx != -1) {
        String wbStr = extractJsonStr(payload, worldIdx, "\"buy\":");
        String wsStr = extractJsonStr(payload, worldIdx, "\"sell\":");
        if (wbStr.length() > 0) strncpy(outInfo.world_buy_str, wbStr.c_str(), sizeof(outInfo.world_buy_str) - 1);
        if (wsStr.length() > 0) strncpy(outInfo.world_sell_str, wsStr.c_str(), sizeof(outInfo.world_sell_str) - 1);
    }

    // 4. Gia Xang RON 95
    int ron95Idx = payload.indexOf("\"ron_95\":");
    if (ron95Idx != -1) {
        int price = extractJsonInt(payload, ron95Idx, "\"price\":");
        int diff = extractJsonInt(payload, ron95Idx, "\"diff\":");
        if (price > 0) {
            outInfo.ron95_price = price;
            outInfo.ron95_delta = diff;
            parsed = true;
        }
    }

    // 5. Gia Xang E5 RON 92
    int e5Idx = payload.indexOf("\"e5_ron_92\":");
    if (e5Idx != -1) {
        int price = extractJsonInt(payload, e5Idx, "\"price\":");
        int diff = extractJsonInt(payload, e5Idx, "\"diff\":");
        if (price > 0) {
            outInfo.ron92_price = price;
            outInfo.e5_price = price;
            outInfo.ron92_delta = diff;
            parsed = true;
        }
    }

    // 6. Gia Dau Diesel
    int dieselIdx = payload.indexOf("\"dau_diesel\":");
    if (dieselIdx != -1) {
        int price = extractJsonInt(payload, dieselIdx, "\"price\":");
        int diff = extractJsonInt(payload, dieselIdx, "\"diff\":");
        if (price > 0) {
            outInfo.diesel_price = price;
            outInfo.diesel_delta = diff;
            parsed = true;
        }
    }

    // 7. Gia Dau Mazut
    int madutIdx = payload.indexOf("\"dau_madut\":");
    if (madutIdx != -1) {
        int price = extractJsonInt(payload, madutIdx, "\"price\":");
        int diff = extractJsonInt(payload, madutIdx, "\"diff\":");
        if (price > 0) {
            outInfo.mazut_price = price;
            outInfo.mazut_delta = diff;
            parsed = true;
        }
    }

    return parsed;
}

static bool executeFetch(MarketInfo &resultInfo) {
    uint32_t maxAlloc = ESP.getMaxAllocHeap();
    String payload = "";
    bool fetchSuccess = false;

    // 1. Thu HTTPS neu heap lien tuc du cho TLS buffer
    const bool canUseTLS = (maxAlloc >= 30 * 1024);
    if (canUseTLS) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(10000);

        const char *host = "gw.vnexpress.net";
        const int port = 443;
        VMD_LOG_I("Connecting TLS to %s:%d...", host, port);

        if (client.connect(host, port)) {
            HTTPClient http;
            http.setTimeout(10000);
            http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

            const char *httpsUrl = "https://gw.vnexpress.net/th?types=gia_vang_v2,gia_xang_dau";
            if (http.begin(client, httpsUrl)) {
                int httpCode = http.GET();
                if (httpCode == HTTP_CODE_OK) {
                    payload = http.getString();
                    fetchSuccess = true;
                    VMD_LOG_I("HTTPS fetch OK, length: %d", payload.length());
                } else {
                    VMD_LOG_W("HTTPS Error: %d", httpCode);
                }
                http.end();
            }
        }
        client.stop();
    }

    // 2. HTTP Fallback qua port 80
    if (!fetchSuccess) {
        VMD_LOG_W("Trying HTTP fallback (port 80)...");
        HTTPClient httpPlain;
        httpPlain.setTimeout(8000);
        httpPlain.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        httpPlain.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        const char *httpUrl = "http://gw.vnexpress.net/th?types=gia_vang_v2,gia_xang_dau";
        if (httpPlain.begin(httpUrl)) {
            int httpCode = httpPlain.GET();
            if (httpCode == HTTP_CODE_OK) {
                payload = httpPlain.getString();
                fetchSuccess = true;
                VMD_LOG_I("HTTP fallback OK, length: %d", payload.length());
            } else {
                VMD_LOG_W("HTTP fallback Error: %d", httpCode);
            }
            httpPlain.end();
        }
    }

    if (fetchSuccess && payload.length() > 0) {
        return parseMarketPayload(payload, resultInfo);
    }
    return false;
}

void VietnameseMarketData::init() {
    if (marketMutex == NULL) {
        marketMutex = xSemaphoreCreateMutex();
    }
    last_fetch_time = 0;
    is_fetching = false;
}

void VietnameseMarketData::setUpdateInterval(uint32_t intervalMinutes) {
    if (intervalMinutes < 5) intervalMinutes = 5;
    update_interval_ms = intervalMinutes * 60 * 1000;
}

void VietnameseMarketData::fetchMarketTask(void *param) {
    MarketInfo info;
    if (executeFetch(info)) {
        if (marketMutex != NULL && xSemaphoreTake(marketMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            info.is_valid = true;
            info.last_update_time = millis();
            current_market = info;
            xSemaphoreGive(marketMutex);
        }
    }
    is_fetching = false;
    vTaskDelete(NULL);
}

void VietnameseMarketData::update(bool wifiConnected, bool force, uint32_t intervalMinutes) {
    if (!wifiConnected) return;

    uint32_t interval = (intervalMinutes > 0) ? (intervalMinutes * 60 * 1000) : update_interval_ms;
    if (!current_market.is_valid) {
        interval = 20 * 1000; // Retry nhanh sau 20s neu chua co du lieu
    }

    uint32_t now = millis();
    if (!is_fetching && (force || last_fetch_time == 0 || (now - last_fetch_time >= interval))) {
        last_fetch_time = now;
        is_fetching = true;

        BaseType_t res = xTaskCreatePinnedToCore(
            fetchMarketTask,
            "VnMarketTask",
            12 * 1024,
            NULL,
            1,
            NULL,
            0
        );
        if (res != pdPASS) {
            VMD_LOG_E("Task creation failed");
            is_fetching = false;
        }
    }
}

bool VietnameseMarketData::fetchNow() {
    MarketInfo info;
    if (executeFetch(info)) {
        if (marketMutex != NULL && xSemaphoreTake(marketMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            info.is_valid = true;
            info.last_update_time = millis();
            current_market = info;
            xSemaphoreGive(marketMutex);
            return true;
        }
    }
    return false;
}

MarketInfo VietnameseMarketData::getMarket() {
    MarketInfo copy;
    if (marketMutex != NULL && xSemaphoreTake(marketMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = current_market;
        xSemaphoreGive(marketMutex);
    }
    return copy;
}
