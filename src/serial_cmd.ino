// ── 시리얼 커맨드 처리 ──────────────────────────
// USB 시리얼을 통한 설정값 입력/조회 인터페이스

#include <Arduino.h>
#include "config.h"

extern uint8_t g_psk[];
extern uint8_t g_beacon_uuid[];
extern uint8_t g_service_uuid[];
extern char g_device_name[];
extern bool g_configured;
extern beacon_state_t g_state;

// 시리얼 입력 버퍼
static char s_serial_buf[128];
static int s_serial_pos = 0;

// NVS 함수 전방 선언
void nvs_save_psk(const uint8_t* psk);
void nvs_save_service_uuid(const uint8_t* uuid);
void nvs_save_device_name(const char* name);
void nvs_mark_configured();
void nvs_reset();

// ── 헬퍼 ────────────────────────────────────────

void print_hex(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
    }
}

// ── 커맨드 파싱 ─────────────────────────────────

serial_cmd_t parse_command(const char* line) {
    if (strncmp(line, "SET_PSK ", 8) == 0)            return CMD_SET_PSK;
    if (strncmp(line, "SET_SERVICE_UUID ", 17) == 0)  return CMD_SET_SERVICE_UUID;
    if (strncmp(line, "SET_NAME ", 9) == 0)           return CMD_SET_NAME;
    if (strcmp(line, "GET_CONFIG") == 0)               return CMD_GET_CONFIG;
    if (strcmp(line, "RESET_CONFIG") == 0)             return CMD_RESET_CONFIG;
    return CMD_UNKNOWN;
}

// ── 커맨드 핸들러 ───────────────────────────────

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

void handle_set_name(const char* arg) {
    if (strlen(arg) == 0 || strlen(arg) > MAX_DEVICE_NAME_LEN) {
        Serial.print("ERROR: 이름은 1~");
        Serial.print(MAX_DEVICE_NAME_LEN);
        Serial.println("자 사이여야 합니다.");
        return;
    }
    nvs_save_device_name(arg);
    Serial.print("OK: Device Name = ");
    Serial.println(g_device_name);
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

    Serial.print("Device Name: ");
    Serial.println(g_device_name);

    Serial.print("State: ");
    Serial.println(g_state == STATE_IDLE ? "IDLE" : "ACTIVE");
    Serial.println("=================");
}

// ── 메인 처리 ───────────────────────────────────

void process_serial_line(const char* line) {
    serial_cmd_t cmd = parse_command(line);

    switch (cmd) {
        case CMD_SET_PSK:
            handle_set_psk(line + 8);
            break;
        case CMD_SET_SERVICE_UUID:
            handle_set_service_uuid(line + 17);
            break;
        case CMD_SET_NAME:
            handle_set_name(line + 9);
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
            Serial.println("  SET_NAME <이름>");
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
            if (s_serial_pos > 0) {
                s_serial_buf[s_serial_pos] = '\0';
                process_serial_line(s_serial_buf);
                s_serial_pos = 0;
            }
        } else if (s_serial_pos < (int)sizeof(s_serial_buf) - 1) {
            s_serial_buf[s_serial_pos++] = c;
        }
    }
}
