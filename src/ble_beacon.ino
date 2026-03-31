// ── BLE iBeacon 광고 ────────────────────────────
// NimBLE Extended Advertising으로 고정 비콘 iBeacon 신호를 송출

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>
#include "config.h"

extern uint8_t g_beacon_uuid[];
extern char g_device_name[];
extern beacon_state_t g_state;

// gatt_server.ino 전방 선언
bool gatt_init();

// Extended Advertising 인스턴스 ID
#define ADV_INSTANCE_FIXED    0   // 고정 비콘 (항상)
#define ADV_INSTANCE_SESSION  1   // 출석 비콘 (ACTIVE 시에만, Step 5-6에서 구현)

NimBLEExtAdvertising* g_pAdvertising = nullptr;
TimerHandle_t g_sessionTimer = nullptr;

// iBeacon 광고 데이터를 생성하여 NimBLEExtAdvertisement에 설정
void build_ibeacon_adv(NimBLEExtAdvertisement& adv, const uint8_t* uuid) {
    NimBLEBeacon beacon;

    // UUID를 NimBLEUUID 형식 문자열로 변환 (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2],  uuid[3],
        uuid[4], uuid[5], uuid[6],  uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);

    beacon.setProximityUUID(NimBLEUUID(uuid_str));
    beacon.setMajor(0);
    beacon.setMinor(0);
    beacon.setSignalPower(-56);  // RSSI at 1 meter (보정용 참고값)

    // iBeacon 광고 데이터 구성: Manufacturer Specific Data
    // [Company ID 2B][Beacon Type 2B][UUID 16B][Major 2B][Minor 2B][TX Power 1B]
    const NimBLEBeacon::BeaconData& data = beacon.getData();

    adv.setLegacyAdvertising(true);  // iBeacon은 Legacy 광고 필요
    adv.setConnectable(false);       // iBeacon은 연결 불가
    adv.setScannable(false);

    // Manufacturer Specific Data로 설정
    adv.setManufacturerData((const uint8_t*)&data, sizeof(data));

    adv.setMinInterval(ADV_INTERVAL_MIN);
    adv.setMaxInterval(ADV_INTERVAL_MAX);
}

// BLE 초기화 및 고정 비콘 광고 시작
bool ble_init_and_start() {
    NimBLEDevice::init(g_device_name);

    g_pAdvertising = NimBLEDevice::getAdvertising();
    if (!g_pAdvertising) {
        Serial.println("ERROR: Extended Advertising 초기화 실패");
        return false;
    }

    // 고정 비콘 광고 세트 구성
    NimBLEExtAdvertisement fixedAdv(BLE_HCI_LE_PHY_1M, BLE_HCI_LE_PHY_1M);
    build_ibeacon_adv(fixedAdv, g_beacon_uuid);

    if (!g_pAdvertising->setInstanceData(ADV_INSTANCE_FIXED, fixedAdv)) {
        Serial.println("ERROR: 고정 비콘 광고 데이터 설정 실패");
        return false;
    }

    // 고정 비콘 광고 시작 (duration=0: 무한)
    if (!g_pAdvertising->start(ADV_INSTANCE_FIXED, 0, 0)) {
        Serial.println("ERROR: 고정 비콘 광고 시작 실패");
        return false;
    }

    Serial.println("BLE 고정 비콘 광고 시작됨");

    // GATT 서버 초기화
    if (!gatt_init()) {
        return false;
    }

    return true;
}

// ── 출석 비콘 (세션) ────────────────────────────

// 출석 비콘 광고 종료
void stop_session_beacon() {
    if (g_state != STATE_ACTIVE) return;

    g_pAdvertising->stop(ADV_INSTANCE_SESSION);
    g_state = STATE_IDLE;

    Serial.println("BLE: 출석 비콘 광고 종료, IDLE 복귀");
}

// FreeRTOS 타이머 콜백: Duration 만료 시 출석 비콘을 자동 종료
void session_timer_callback(TimerHandle_t xTimer) {
    stop_session_beacon();
}

// 출석 비콘 광고 시작
void start_session_beacon(const uint8_t* session_uuid, uint16_t duration_sec) {
    // 이미 ACTIVE면 기존 세션 중단
    if (g_state == STATE_ACTIVE) {
        Serial.println("BLE: 기존 출석 세션 중단, 새 세션 시작");
        g_pAdvertising->stop(ADV_INSTANCE_SESSION);
        if (g_sessionTimer) {
            xTimerStop(g_sessionTimer, 0);
        }
    }

    // 출석 비콘 광고 세트 구성
    NimBLEExtAdvertisement sessionAdv(BLE_HCI_LE_PHY_1M, BLE_HCI_LE_PHY_1M);
    build_ibeacon_adv(sessionAdv, session_uuid);

    if (!g_pAdvertising->setInstanceData(ADV_INSTANCE_SESSION, sessionAdv)) {
        Serial.println("ERROR: 출석 비콘 광고 데이터 설정 실패");
        return;
    }

    if (!g_pAdvertising->start(ADV_INSTANCE_SESSION, 0, 0)) {
        Serial.println("ERROR: 출석 비콘 광고 시작 실패");
        return;
    }

    g_state = STATE_ACTIVE;

    // FreeRTOS 타이머로 자동 종료 설정
    if (g_sessionTimer == nullptr) {
        g_sessionTimer = xTimerCreate(
            "session", pdMS_TO_TICKS(duration_sec * 1000),
            pdFALSE, nullptr, session_timer_callback
        );
    } else {
        xTimerChangePeriod(g_sessionTimer, pdMS_TO_TICKS(duration_sec * 1000), 0);
    }
    xTimerStart(g_sessionTimer, 0);

    Serial.print("BLE: 출석 비콘 광고 시작 (");
    Serial.print(duration_sec);
    Serial.println("초 후 자동 종료)");
}
