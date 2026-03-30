#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ── 비콘 상태 ────────────────────────────────────
typedef enum {
    STATE_IDLE,    // 고정 비콘만 광고 중 (평소)
    STATE_ACTIVE   // 고정 비콘 + 출석 비콘 동시 광고 중
} beacon_state_t;

// ── iBeacon 포맷 상수 ────────────────────────────
#define IBEACON_COMPANY_ID    0x004C  // Apple 제조사 ID (iBeacon 표준)

// ── UUID / PSK 크기 ─────────────────────────────
#define UUID_LENGTH           16
#define PSK_LENGTH            16

// ── 고정 비콘 UUID 접두사 ────────────────────────
// MAC(6B)와 결합하여 16B UUID 자동 생성: "MAMOKEY-BN" + MAC
#define UUID_PREFIX           { 0x4D, 0x41, 0x4D, 0x4F, 0x4B, 0x45, 0x59, 0x2D, 0x42, 0x4E }
#define UUID_PREFIX_LENGTH    10

// ── GATT 페이로드 ───────────────────────────────
// [PSK 16B][Session UUID 16B][Duration 2B] = 34바이트
#define GATT_PAYLOAD_LENGTH   (PSK_LENGTH + UUID_LENGTH + 2)
#define PAYLOAD_PSK_OFFSET    0
#define PAYLOAD_UUID_OFFSET   PSK_LENGTH
#define PAYLOAD_DURATION_OFFSET (PSK_LENGTH + UUID_LENGTH)

// ── GATT 특성 UUID (고정) ───────────────────────
// 서비스 내부 기능 식별용. 서비스 UUID가 동아리를 구분하므로 고정.
#define GATT_CHAR_UUID        "23a02cbe-b3fd-4c39-8565-406325f2bf4e"

// ── BLE 광고 설정 ───────────────────────────────
// 0.625ms 단위. 160 = 100ms (iBeacon 표준)
#define ADV_INTERVAL_MIN      160
#define ADV_INTERVAL_MAX      160

// ── NVS 설정 ────────────────────────────────────
#define NVS_NAMESPACE         "beacon"
#define NVS_KEY_PSK           "psk"
#define NVS_KEY_SERVICE_UUID  "svc_uuid"
#define NVS_KEY_DEVICE_NAME   "dev_name"
#define NVS_KEY_CONFIGURED    "configured"

// ── BLE 디바이스 이름 ───────────────────────────
#define DEFAULT_DEVICE_NAME   "MAMOKEY-Beacon"
#define MAX_DEVICE_NAME_LEN   32

// ── LED 설정 ────────────────────────────────────
#define LED_PIN               48
#define LED_COUNT             1

// ── 순수 로직 함수 ──────────────────────────────

static inline int validate_payload(uint16_t length) {
    return length == GATT_PAYLOAD_LENGTH;
}

static inline int verify_psk(const uint8_t* received, const uint8_t* stored) {
    for (int i = 0; i < PSK_LENGTH; i++) {
        if (received[i] != stored[i]) return 0;
    }
    return 1;
}

static inline uint16_t parse_duration(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline void parse_payload(const uint8_t* payload,
                                 const uint8_t** psk,
                                 const uint8_t** uuid,
                                 uint16_t* duration) {
    *psk = payload + PAYLOAD_PSK_OFFSET;
    *uuid = payload + PAYLOAD_UUID_OFFSET;
    *duration = parse_duration(payload + PAYLOAD_DURATION_OFFSET);
}

static inline int is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static inline int validate_hex_string(const char* str, int expected_len) {
    int i;
    for (i = 0; i < expected_len; i++) {
        if (str[i] == '\0' || !is_hex_char(str[i])) return 0;
    }
    if (str[i] != '\0') return 0;
    return 1;
}

static inline uint8_t hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static inline void hex_string_to_bytes(const char* hex, uint8_t* bytes, int byte_count) {
    for (int i = 0; i < byte_count; i++) {
        bytes[i] = (hex_char_to_nibble(hex[i * 2]) << 4) |
                    hex_char_to_nibble(hex[i * 2 + 1]);
    }
}

static inline void generate_uuid_from_mac(const uint8_t* mac, uint8_t* uuid) {
    const uint8_t prefix[] = UUID_PREFIX;
    for (int i = 0; i < UUID_PREFIX_LENGTH; i++) {
        uuid[i] = prefix[i];
    }
    for (int i = 0; i < 6; i++) {
        uuid[UUID_PREFIX_LENGTH + i] = mac[i];
    }
}

// ── 시리얼 커맨드 ───────────────────────────────
typedef enum {
    CMD_UNKNOWN,
    CMD_SET_PSK,
    CMD_SET_SERVICE_UUID,
    CMD_SET_NAME,
    CMD_GET_CONFIG,
    CMD_RESET_CONFIG
} serial_cmd_t;

#endif // CONFIG_H
