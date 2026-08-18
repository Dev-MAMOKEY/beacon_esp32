#include <unity.h>
#include <string.h>
#include "config.h"

// serial_cmd.ino의 parse_command는 Arduino 의존이 없는 순수 함수이므로 재정의
static serial_cmd_t parse_command(const char* line) {
    if (strncmp(line, "SET_PSK ", 8) == 0)            return CMD_SET_PSK;
    if (strncmp(line, "SET_SERVICE_UUID ", 17) == 0)   return CMD_SET_SERVICE_UUID;
    if (strncmp(line, "SET_NAME ", 9) == 0)            return CMD_SET_NAME;
    if (strcmp(line, "GET_CONFIG") == 0)                return CMD_GET_CONFIG;
    if (strcmp(line, "RESET_CONFIG") == 0)              return CMD_RESET_CONFIG;
    return CMD_UNKNOWN;
}

void setUp(void) {}
void tearDown(void) {}

// ── validate_payload 테스트 ─────────────────────

void test_validate_payload_correct_length(void) {
    TEST_ASSERT_TRUE(validate_payload(GATT_PAYLOAD_LENGTH));
}

void test_validate_payload_too_short(void) {
    TEST_ASSERT_FALSE(validate_payload(GATT_PAYLOAD_LENGTH - 1));
}

void test_validate_payload_too_long(void) {
    TEST_ASSERT_FALSE(validate_payload(GATT_PAYLOAD_LENGTH + 1));
}

void test_validate_payload_zero(void) {
    TEST_ASSERT_FALSE(validate_payload(0));
}

// ── verify_psk 테스트 ───────────────────────────

void test_verify_psk_match(void) {
    uint8_t psk[PSK_LENGTH] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    TEST_ASSERT_TRUE(verify_psk(psk, psk));
}

void test_verify_psk_mismatch(void) {
    uint8_t received[PSK_LENGTH] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                     0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    uint8_t stored[PSK_LENGTH]   = {0xFF, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                     0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    TEST_ASSERT_FALSE(verify_psk(received, stored));
}

void test_verify_psk_last_byte_different(void) {
    uint8_t received[PSK_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                                     0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t stored[PSK_LENGTH]   = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                                     0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x00};
    TEST_ASSERT_FALSE(verify_psk(received, stored));
}

// ── parse_duration 테스트 ───────────────────────

void test_parse_duration_basic(void) {
    uint8_t data[2] = {0x3C, 0x00};  // 60초 (little-endian)
    TEST_ASSERT_EQUAL_UINT16(60, parse_duration(data));
}

void test_parse_duration_max(void) {
    uint8_t data[2] = {0xFF, 0xFF};  // 65535
    TEST_ASSERT_EQUAL_UINT16(65535, parse_duration(data));
}

void test_parse_duration_zero(void) {
    uint8_t data[2] = {0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT16(0, parse_duration(data));
}

void test_parse_duration_high_byte(void) {
    uint8_t data[2] = {0x00, 0x01};  // 256 (little-endian)
    TEST_ASSERT_EQUAL_UINT16(256, parse_duration(data));
}

// ── parse_payload 테스트 ────────────────────────

void test_parse_payload_splits_correctly(void) {
    uint8_t payload[GATT_PAYLOAD_LENGTH];
    // PSK: 0x01~0x10
    for (int i = 0; i < PSK_LENGTH; i++) payload[i] = i + 1;
    // UUID: 0xA1~0xB0
    for (int i = 0; i < UUID_LENGTH; i++) payload[PSK_LENGTH + i] = 0xA0 + i + 1;
    // Duration: 120초 (0x78, 0x00 little-endian)
    payload[PAYLOAD_DURATION_OFFSET] = 0x78;
    payload[PAYLOAD_DURATION_OFFSET + 1] = 0x00;

    const uint8_t* psk;
    const uint8_t* uuid;
    uint16_t duration;
    parse_payload(payload, &psk, &uuid, &duration);

    TEST_ASSERT_EQUAL_UINT8(0x01, psk[0]);
    TEST_ASSERT_EQUAL_UINT8(0x10, psk[PSK_LENGTH - 1]);
    TEST_ASSERT_EQUAL_UINT8(0xA1, uuid[0]);
    TEST_ASSERT_EQUAL_UINT8(0xB0, uuid[UUID_LENGTH - 1]);
    TEST_ASSERT_EQUAL_UINT16(120, duration);
}

// ── validate_hex_string 테스트 ──────────────────

void test_validate_hex_string_valid_32(void) {
    TEST_ASSERT_TRUE(validate_hex_string("0123456789abcdef0123456789ABCDEF", 32));
}

void test_validate_hex_string_too_short(void) {
    TEST_ASSERT_FALSE(validate_hex_string("0123", 32));
}

void test_validate_hex_string_too_long(void) {
    TEST_ASSERT_FALSE(validate_hex_string("0123456789abcdef0123456789ABCDEF00", 32));
}

void test_validate_hex_string_invalid_char(void) {
    TEST_ASSERT_FALSE(validate_hex_string("0123456789abcdeg0123456789ABCDEF", 32));
}

void test_validate_hex_string_empty(void) {
    TEST_ASSERT_FALSE(validate_hex_string("", 32));
}

void test_validate_hex_string_all_digits(void) {
    TEST_ASSERT_TRUE(validate_hex_string("1234567890", 10));
}

// ── hex_string_to_bytes 테스트 ──────────────────

void test_hex_string_to_bytes(void) {
    uint8_t result[4];
    hex_string_to_bytes("DEADBEEF", result, 4);
    TEST_ASSERT_EQUAL_UINT8(0xDE, result[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, result[1]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, result[2]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, result[3]);
}

void test_hex_string_to_bytes_lowercase(void) {
    uint8_t result[2];
    hex_string_to_bytes("ff00", result, 2);
    TEST_ASSERT_EQUAL_UINT8(0xFF, result[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, result[1]);
}

// ── generate_uuid_from_mac 테스트 ───────────────

void test_generate_uuid_from_mac(void) {
    uint8_t mac[6] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F};
    uint8_t uuid[UUID_LENGTH];
    generate_uuid_from_mac(mac, uuid);

    // 접두사 "MAMOKEY-BN" 확인
    const uint8_t expected_prefix[] = {0x4D, 0x41, 0x4D, 0x4F, 0x4B, 0x45, 0x59, 0x2D, 0x42, 0x4E};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_prefix, uuid, UUID_PREFIX_LENGTH);

    // MAC 부분 확인
    TEST_ASSERT_EQUAL_UINT8_ARRAY(mac, uuid + UUID_PREFIX_LENGTH, 6);
}

// ── parse_command 테스트 ────────────────────────

void test_parse_command_set_psk(void) {
    TEST_ASSERT_EQUAL(CMD_SET_PSK, parse_command("SET_PSK 0123456789abcdef0123456789abcdef"));
}

void test_parse_command_set_service_uuid(void) {
    TEST_ASSERT_EQUAL(CMD_SET_SERVICE_UUID, parse_command("SET_SERVICE_UUID fedcba9876543210fedcba9876543210"));
}

void test_parse_command_set_name(void) {
    TEST_ASSERT_EQUAL(CMD_SET_NAME, parse_command("SET_NAME MyBeacon"));
}

void test_parse_command_get_config(void) {
    TEST_ASSERT_EQUAL(CMD_GET_CONFIG, parse_command("GET_CONFIG"));
}

void test_parse_command_reset_config(void) {
    TEST_ASSERT_EQUAL(CMD_RESET_CONFIG, parse_command("RESET_CONFIG"));
}

void test_parse_command_unknown(void) {
    TEST_ASSERT_EQUAL(CMD_UNKNOWN, parse_command("INVALID_CMD"));
}

void test_parse_command_empty(void) {
    TEST_ASSERT_EQUAL(CMD_UNKNOWN, parse_command(""));
}

// ── main ────────────────────────────────────────

int main(void) {
    UNITY_BEGIN();

    // validate_payload
    RUN_TEST(test_validate_payload_correct_length);
    RUN_TEST(test_validate_payload_too_short);
    RUN_TEST(test_validate_payload_too_long);
    RUN_TEST(test_validate_payload_zero);

    // verify_psk
    RUN_TEST(test_verify_psk_match);
    RUN_TEST(test_verify_psk_mismatch);
    RUN_TEST(test_verify_psk_last_byte_different);

    // parse_duration
    RUN_TEST(test_parse_duration_basic);
    RUN_TEST(test_parse_duration_max);
    RUN_TEST(test_parse_duration_zero);
    RUN_TEST(test_parse_duration_high_byte);

    // parse_payload
    RUN_TEST(test_parse_payload_splits_correctly);

    // validate_hex_string
    RUN_TEST(test_validate_hex_string_valid_32);
    RUN_TEST(test_validate_hex_string_too_short);
    RUN_TEST(test_validate_hex_string_too_long);
    RUN_TEST(test_validate_hex_string_invalid_char);
    RUN_TEST(test_validate_hex_string_empty);
    RUN_TEST(test_validate_hex_string_all_digits);

    // hex_string_to_bytes
    RUN_TEST(test_hex_string_to_bytes);
    RUN_TEST(test_hex_string_to_bytes_lowercase);

    // generate_uuid_from_mac
    RUN_TEST(test_generate_uuid_from_mac);

    // parse_command
    RUN_TEST(test_parse_command_set_psk);
    RUN_TEST(test_parse_command_set_service_uuid);
    RUN_TEST(test_parse_command_set_name);
    RUN_TEST(test_parse_command_get_config);
    RUN_TEST(test_parse_command_reset_config);
    RUN_TEST(test_parse_command_unknown);
    RUN_TEST(test_parse_command_empty);

    return UNITY_END();
}
