// ── BLE iBeacon 광고 ────────────────────────────
// NimBLE Extended Advertising으로 고정 비콘 iBeacon 신호를 송출

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEBeacon.h>
#include "config.h"

extern uint8_t g_beacon_uuid[];
extern char g_device_name[];

// Extended Advertising 인스턴스 ID
#define ADV_INSTANCE_FIXED    0   // 고정 비콘 (항상)
#define ADV_INSTANCE_SESSION  1   // 출석 비콘 (ACTIVE 시에만, Step 5-6에서 구현)

NimBLEExtAdvertising* g_pAdvertising = nullptr;

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
    return true;
}
