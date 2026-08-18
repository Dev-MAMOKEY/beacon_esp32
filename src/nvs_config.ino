// ── NVS 설정 관리 ───────────────────────────────
// ESP32의 플래시 메모리에 설정값을 영구 저장/로드하는 함수들

#include <Preferences.h>
#include "config.h"

extern Preferences preferences;
extern uint8_t g_psk[];
extern uint8_t g_service_uuid[];
extern char g_device_name[];
extern bool g_configured;

void nvs_load_config() {
    preferences.begin(NVS_NAMESPACE, true);  // true = 읽기 전용

    g_configured = preferences.getBool(NVS_KEY_CONFIGURED, false);

    if (g_configured) {
        preferences.getBytes(NVS_KEY_PSK, g_psk, PSK_LENGTH);
        preferences.getBytes(NVS_KEY_SERVICE_UUID, g_service_uuid, UUID_LENGTH);
    }

    // 디바이스 이름은 필수 설정과 무관하게 로드
    String name = preferences.getString(NVS_KEY_DEVICE_NAME, DEFAULT_DEVICE_NAME);
    strncpy(g_device_name, name.c_str(), MAX_DEVICE_NAME_LEN);
    g_device_name[MAX_DEVICE_NAME_LEN] = '\0';

    preferences.end();
}

void nvs_save_psk(const uint8_t* psk) {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes(NVS_KEY_PSK, psk, PSK_LENGTH);
    preferences.end();
    memcpy(g_psk, psk, PSK_LENGTH);
}

void nvs_save_service_uuid(const uint8_t* uuid) {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBytes(NVS_KEY_SERVICE_UUID, uuid, UUID_LENGTH);
    preferences.end();
    memcpy(g_service_uuid, uuid, UUID_LENGTH);
}

void nvs_save_device_name(const char* name) {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putString(NVS_KEY_DEVICE_NAME, name);
    preferences.end();
    strncpy(g_device_name, name, MAX_DEVICE_NAME_LEN);
    g_device_name[MAX_DEVICE_NAME_LEN] = '\0';
}

void nvs_mark_configured() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBool(NVS_KEY_CONFIGURED, true);
    preferences.end();
    g_configured = true;
}

void nvs_reset() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();

    memset(g_psk, 0, PSK_LENGTH);
    memset(g_service_uuid, 0, UUID_LENGTH);
    strncpy(g_device_name, DEFAULT_DEVICE_NAME, MAX_DEVICE_NAME_LEN);
    g_configured = false;

    Serial.println("OK: 설정이 초기화되었습니다. 재부팅하세요.");
}
