// ── GATT 서버 ───────────────────────────────────
// 관리자 앱이 BLE로 출석 시작 명령을 보내는 인터페이스
// Write Characteristic에 34바이트 페이로드를 쓰면 출석 비콘이 시작됨

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"

extern uint8_t g_psk[];
extern uint8_t g_service_uuid[];
extern beacon_state_t g_state;

// ble_beacon.ino의 함수 전방 선언
void start_session_beacon(const uint8_t* session_uuid, uint16_t duration_sec);
void stop_session_beacon();

// GATT Write 콜백: 관리자 앱이 출석 시작 명령을 보낼 때 호출됨
class AttendanceCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();

        // 페이로드 길이 검증
        if (!validate_payload(value.length())) {
            Serial.println("GATT: 잘못된 페이로드 길이");
            return;
        }

        const uint8_t* payload = (const uint8_t*)value.data();
        const uint8_t* received_psk;
        const uint8_t* session_uuid;
        uint16_t duration;

        parse_payload(payload, &received_psk, &session_uuid, &duration);

        // PSK 검증
        if (!verify_psk(received_psk, g_psk)) {
            Serial.println("GATT: PSK 불일치, 명령 무시");
            return;
        }

        // Duration 유효성 검사 (1초 ~ 600초)
        if (duration == 0 || duration > 600) {
            Serial.println("GATT: 잘못된 Duration 값");
            return;
        }

        Serial.print("GATT: 출석 시작, Duration = ");
        Serial.print(duration);
        Serial.println("초");

        // 출석 비콘 광고 시작
        start_session_beacon(session_uuid, duration);
    }
};

static AttendanceCallbacks attendanceCallbacks;

// GATT 서버 초기화: 서비스와 Write Characteristic 생성
bool gatt_init() {
    // 서비스 UUID를 NVS에서 로드한 바이트 배열로 구성
    char svc_uuid_str[37];
    snprintf(svc_uuid_str, sizeof(svc_uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        g_service_uuid[0],  g_service_uuid[1],  g_service_uuid[2],  g_service_uuid[3],
        g_service_uuid[4],  g_service_uuid[5],  g_service_uuid[6],  g_service_uuid[7],
        g_service_uuid[8],  g_service_uuid[9],  g_service_uuid[10], g_service_uuid[11],
        g_service_uuid[12], g_service_uuid[13], g_service_uuid[14], g_service_uuid[15]);

    NimBLEServer* pServer = NimBLEDevice::createServer();
    if (!pServer) {
        Serial.println("ERROR: GATT 서버 생성 실패");
        return false;
    }

    NimBLEService* pService = pServer->createService(svc_uuid_str);
    if (!pService) {
        Serial.println("ERROR: GATT 서비스 생성 실패");
        return false;
    }

    NimBLECharacteristic* pCharacteristic = pService->createCharacteristic(
        GATT_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pCharacteristic->setCallbacks(&attendanceCallbacks);

    Serial.println("GATT 서버 시작됨");
    return true;
}
