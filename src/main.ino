#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ── 전역 변수 ───────────────────────────────────
Preferences preferences;

// NVS에서 로드된 설정값
uint8_t g_psk[PSK_LENGTH];
uint8_t g_beacon_uuid[UUID_LENGTH];    // MAC 기반 자동 생성
uint8_t g_service_uuid[UUID_LENGTH];   // GATT 서비스 UUID (시리얼 설정)
uint16_t g_major = DEFAULT_MAJOR;
uint16_t g_minor = DEFAULT_MINOR;
bool g_configured = false;             // 필수 설정(PSK, 서비스 UUID)이 완료됐는지

beacon_state_t g_state = STATE_IDLE;

// 시리얼 입력 버퍼
char g_serial_buf[128];
int g_serial_pos = 0;

// ── NVS 함수 ────────────────────────────────────

void nvs_load_config() {
    preferences.begin(NVS_NAMESPACE, true);  // true = 읽기 전용

    g_configured = preferences.getBool(NVS_KEY_CONFIGURED, false);
    g_major = preferences.getUShort(NVS_KEY_MAJOR, DEFAULT_MAJOR);
    g_minor = preferences.getUShort(NVS_KEY_MINOR, DEFAULT_MINOR);

    if (g_configured) {
        preferences.getBytes(NVS_KEY_PSK, g_psk, PSK_LENGTH);
        preferences.getBytes(NVS_KEY_SERVICE_UUID, g_service_uuid, UUID_LENGTH);
    }

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

void nvs_save_major(uint16_t major) {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putUShort(NVS_KEY_MAJOR, major);
    preferences.end();
    g_major = major;
}

void nvs_save_minor(uint16_t minor) {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putUShort(NVS_KEY_MINOR, minor);
    preferences.end();
    g_minor = minor;
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
    g_major = DEFAULT_MAJOR;
    g_minor = DEFAULT_MINOR;
    g_configured = false;

    Serial.println("OK: 설정이 초기화되었습니다. 재부팅하세요.");
}

// ── UUID 출력 헬퍼 ──────────────────────────────

void print_hex(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
    }
}

// ── 시리얼 커맨드 처리 ──────────────────────────

serial_cmd_t parse_command(const char* line) {
    if (strncmp(line, "SET_PSK ", 8) == 0)          return CMD_SET_PSK;
    if (strncmp(line, "SET_MAJOR ", 10) == 0)        return CMD_SET_MAJOR;
    if (strncmp(line, "SET_MINOR ", 10) == 0)        return CMD_SET_MINOR;
    if (strncmp(line, "SET_SERVICE_UUID ", 17) == 0)  return CMD_SET_SERVICE_UUID;
    if (strcmp(line, "GET_CONFIG") == 0)              return CMD_GET_CONFIG;
    if (strcmp(line, "RESET_CONFIG") == 0)            return CMD_RESET_CONFIG;
    return CMD_UNKNOWN;
}

void handle_set_psk(const char* arg) {
    if (!validate_hex_string(arg, PSK_LENGTH * 2)) {
        Serial.println("ERROR: PSK는 32자리 16진수여야 합니다.");
        return;
    }
    uint8_t psk[PSK_LENGTH];
    hex_string_to_bytes(arg, psk, PSK_LENGTH);
    nvs_save_psk(psk);
    Serial.print("OK: PSK = ");
    print_hex(psk, PSK_LENGTH);
    Serial.println();
}

void handle_set_service_uuid(const char* arg) {
    if (!validate_hex_string(arg, UUID_LENGTH * 2)) {
        Serial.println("ERROR: UUID는 32자리 16진수여야 합니다.");
        return;
    }
    uint8_t uuid[UUID_LENGTH];
    hex_string_to_bytes(arg, uuid, UUID_LENGTH);
    nvs_save_service_uuid(uuid);
    Serial.print("OK: Service UUID = ");
    print_hex(uuid, UUID_LENGTH);
    Serial.println();
}

void handle_set_major(const char* arg) {
    long val = atol(arg);
    if (val < 0 || val > 65535) {
        Serial.println("ERROR: Major는 0~65535 범위여야 합니다.");
        return;
    }
    nvs_save_major((uint16_t)val);
    Serial.print("OK: Major = ");
    Serial.println(g_major);
}

void handle_set_minor(const char* arg) {
    long val = atol(arg);
    if (val < 0 || val > 65535) {
        Serial.println("ERROR: Minor는 0~65535 범위여야 합니다.");
        return;
    }
    nvs_save_minor((uint16_t)val);
    Serial.print("OK: Minor = ");
    Serial.println(g_minor);
}

void handle_get_config() {
    Serial.println("=== 현재 설정 ===");
    Serial.print("Configured: ");
    Serial.println(g_configured ? "YES" : "NO");

    Serial.print("Beacon UUID (MAC 기반): ");
    print_hex(g_beacon_uuid, UUID_LENGTH);
    Serial.println();

    Serial.print("Service UUID: ");
    if (g_configured) {
        print_hex(g_service_uuid, UUID_LENGTH);
    } else {
        Serial.print("(미설정)");
    }
    Serial.println();

    Serial.print("PSK: ");
    if (g_configured) {
        print_hex(g_psk, PSK_LENGTH);
    } else {
        Serial.print("(미설정)");
    }
    Serial.println();

    Serial.print("Major: ");
    Serial.println(g_major);
    Serial.print("Minor: ");
    Serial.println(g_minor);

    Serial.print("State: ");
    Serial.println(g_state == STATE_IDLE ? "IDLE" : "ACTIVE");
    Serial.println("=================");
}

void process_serial_line(const char* line) {
    serial_cmd_t cmd = parse_command(line);

    switch (cmd) {
        case CMD_SET_PSK:
            handle_set_psk(line + 8);
            break;
        case CMD_SET_SERVICE_UUID:
            handle_set_service_uuid(line + 17);
            break;
        case CMD_SET_MAJOR:
            handle_set_major(line + 10);
            break;
        case CMD_SET_MINOR:
            handle_set_minor(line + 10);
            break;
        case CMD_GET_CONFIG:
            handle_get_config();
            break;
        case CMD_RESET_CONFIG:
            nvs_reset();
            break;
        default:
            Serial.println("ERROR: 알 수 없는 명령입니다.");
            Serial.println("사용 가능한 명령:");
            Serial.println("  SET_PSK <32자리 hex>");
            Serial.println("  SET_SERVICE_UUID <32자리 hex>");
            Serial.println("  SET_MAJOR <0~65535>");
            Serial.println("  SET_MINOR <0~65535>");
            Serial.println("  GET_CONFIG");
            Serial.println("  RESET_CONFIG");
            break;
    }

    // PSK와 서비스 UUID 모두 설정되었으면 configured 플래그 세팅
    if (!g_configured) {
        bool has_psk = false;
        bool has_svc = false;
        for (int i = 0; i < PSK_LENGTH; i++) {
            if (g_psk[i] != 0) { has_psk = true; break; }
        }
        for (int i = 0; i < UUID_LENGTH; i++) {
            if (g_service_uuid[i] != 0) { has_svc = true; break; }
        }
        if (has_psk && has_svc) {
            nvs_mark_configured();
            Serial.println("OK: 필수 설정 완료! 재부팅하면 BLE가 시작됩니다.");
        }
    }
}

void check_serial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (g_serial_pos > 0) {
                g_serial_buf[g_serial_pos] = '\0';
                process_serial_line(g_serial_buf);
                g_serial_pos = 0;
            }
        } else if (g_serial_pos < (int)sizeof(g_serial_buf) - 1) {
            g_serial_buf[g_serial_pos++] = c;
        }
    }
}

// ── Arduino 진입점 ──────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);  // USB CDC 안정화 대기

    Serial.println("MAMOKEY Beacon 부팅 중...");

    // MAC 주소로 고정 비콘 UUID 생성
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    generate_uuid_from_mac(mac, g_beacon_uuid);

    // NVS에서 설정 로드
    nvs_load_config();

    Serial.print("Beacon UUID: ");
    print_hex(g_beacon_uuid, UUID_LENGTH);
    Serial.println();

    if (!g_configured) {
        Serial.println("설정이 필요합니다. 시리얼로 SET_PSK, SET_SERVICE_UUID를 입력하세요.");
    } else {
        Serial.println("설정 완료. BLE 시작 준비.");
        // TODO: BLE 초기화 (Step 4에서 구현)
    }
}

void loop() {
    check_serial();
}
