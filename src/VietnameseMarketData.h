#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

struct MarketInfo {
    char sjc_buy_str[16]   = "-----";
    char sjc_sell_str[16]  = "-----";
    char ring_buy_str[16]  = "-----";
    char ring_sell_str[16] = "-----";
    char world_buy_str[16] = "-----";
    char world_sell_str[16]= "-----";

    int ron95_price = 0;
    int ron95_delta = 0;
    int ron92_price = 0;
    int e5_price = 0;
    int ron92_delta = 0;
    int diesel_price = 0;
    int diesel_delta = 0;
    int mazut_price = 0;
    int mazut_delta = 0;

    bool is_valid = false;
    uint32_t last_update_time = 0;
};

class VietnameseMarketData {
private:
    static MarketInfo current_market;
    static SemaphoreHandle_t marketMutex;
    static uint32_t last_fetch_time;
    static bool is_fetching;
    static uint32_t update_interval_ms;
    static char api_url[128];

    static void fetchMarketTask(void *param);

public:
    static void init(const char *endpoint = "https://gw.vnexpress.net/th?types=gia_vang_v2,gia_xang_dau");
    static void setApiUrl(const char *url);
    static void setUpdateInterval(uint32_t intervalMinutes);
    static void update(bool wifiConnected, bool force = false, uint32_t intervalMinutes = 0);
    static bool fetchNow();
    static MarketInfo getMarket();
    static bool isFetching() { return is_fetching; }
};

// Alias de tuong thich nguoc 100%
using MarketService = VietnameseMarketData;
