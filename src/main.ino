#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include "config.h"

// ── 전역 변수 ───────────────────────────────────
Preferences preferences;

uint8_t g_psk[PSK_LENGTH];
uint8_t g_beacon_uuid[UUID_LENGTH];                          // MAC 기반 자동 생성
uint8_t g_service_uuid[UUID_LENGTH];                         // GATT 서비스 UUID (시리얼 설정)
char g_device_name[MAX_DEVICE_NAME_LEN + 1] = DEFAULT_DEVICE_NAME;  // BLE 디바이스 이름 (시리얼 설정)
bool g_configured = false;                                   // 필수 설정(PSK, 서비스 UUID)이 완료됐는지

beacon_state_t g_state = STATE_IDLE;

// 다른 .ino 파일의 함수 전방 선언
void nvs_load_config();
void check_serial();
bool ble_init_and_start();
void ble_check_events();
void led_init();
void set_led_state(led_state_t state);

// ── Arduino 진입점 ──────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);  // USB CDC 안정화 대기

    Serial.println("MAMOKEY Beacon 부팅 중...");

    // LED 초기화 (설정대기 상태로 시작)
    led_init();

    // MAC 주소로 고정 비콘 UUID 생성
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    generate_uuid_from_mac(mac, g_beacon_uuid);

    // NVS에서 설정 로드
    nvs_load_config();

    Serial.print("Beacon UUID: ");
    // print_hex는 serial_cmd.ino에 있으므로 여기서는 직접 출력
    for (int i = 0; i < UUID_LENGTH; i++) {
        if (g_beacon_uuid[i] < 0x10) Serial.print("0");
        Serial.print(g_beacon_uuid[i], HEX);
    }
    Serial.println();

    if (!g_configured) {
        Serial.println("설정이 필요합니다. 시리얼로 SET_PSK, SET_SERVICE_UUID를 입력하세요.");
        set_led_state(LED_STATE_UNCONFIGURED);
    } else {
        if (ble_init_and_start()) {
            Serial.println("BLE 초기화 완료.");
            set_led_state(LED_STATE_IDLE);
        } else {
            Serial.println("ERROR: BLE 초기화 실패.");
            set_led_state(LED_STATE_ERROR);
        }
    }
}

void loop() {
    check_serial();
    ble_check_events();
}
