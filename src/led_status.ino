// ── RGB LED 상태 표시 ───────────────────────────
// Adafruit NeoPixel (WS2812, GPIO 48)로 비콘 상태를 색상/패턴으로 표시
// FreeRTOS 태스크에서 비동기로 동작하여 BLE 로직을 블로킹하지 않음

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

static Adafruit_NeoPixel s_pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static volatile led_state_t s_led_state = LED_STATE_UNCONFIGURED;
static TaskHandle_t s_ledTaskHandle = nullptr;

// ── 색상 정의 ───────────────────────────────────
static const uint32_t COLOR_YELLOW = Adafruit_NeoPixel::Color(255, 200, 0);
static const uint32_t COLOR_BLUE   = Adafruit_NeoPixel::Color(0, 0, 255);
static const uint32_t COLOR_GREEN  = Adafruit_NeoPixel::Color(0, 255, 0);
static const uint32_t COLOR_RED    = Adafruit_NeoPixel::Color(255, 0, 0);
static const uint32_t COLOR_OFF    = Adafruit_NeoPixel::Color(0, 0, 0);

// ── LED 업데이트 태스크 ─────────────────────────
void led_task(void* pvParameters) {
    bool blink_on = true;
    TickType_t last_toggle = xTaskGetTickCount();

    for (;;) {
        led_state_t state = s_led_state;
        uint32_t color;
        uint32_t blink_interval_ms;

        switch (state) {
            case LED_STATE_UNCONFIGURED:
                color = COLOR_YELLOW;
                blink_interval_ms = 1000;  // 느린 깜빡임
                break;
            case LED_STATE_IDLE:
                color = COLOR_BLUE;
                blink_interval_ms = 0;     // 상시 켜짐
                break;
            case LED_STATE_ACTIVE:
                color = COLOR_GREEN;
                blink_interval_ms = 250;   // 빠른 깜빡임
                break;
            case LED_STATE_ERROR:
                color = COLOR_RED;
                blink_interval_ms = 0;     // 상시 켜짐
                break;
            default:
                color = COLOR_OFF;
                blink_interval_ms = 0;
                break;
        }

        if (blink_interval_ms > 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_toggle) >= pdMS_TO_TICKS(blink_interval_ms)) {
                blink_on = !blink_on;
                last_toggle = now;
            }
            s_pixel.setPixelColor(0, blink_on ? color : COLOR_OFF);
        } else {
            s_pixel.setPixelColor(0, color);
            blink_on = true;
        }

        s_pixel.show();
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms 주기로 업데이트
    }
}

// ── 외부 인터페이스 ─────────────────────────────

void led_init() {
    s_pixel.begin();
    s_pixel.setBrightness(LED_BRIGHTNESS);
    s_pixel.setPixelColor(0, COLOR_OFF);
    s_pixel.show();

    xTaskCreate(led_task, "led", 2048, nullptr, 1, &s_ledTaskHandle);
}

void set_led_state(led_state_t state) {
    s_led_state = state;
}
