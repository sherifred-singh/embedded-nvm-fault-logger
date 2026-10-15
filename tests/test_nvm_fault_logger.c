/**
 * @file test_nvm_fault_logger.c
 * @brief Automated Unity Unit Test Suite for NVM Blackbox Fault Logger
 * @author Sherifred Singh
 */

#include "nvm_fault_logger.h"
#include "unity.h"
#include <string.h>

#define TEST_FAKE_NVM_BYTES 4096U

static uint8_t g_fake_nvm[TEST_FAKE_NVM_BYTES];

static uint32_t fake_nvm_read(uint32_t address, uint8_t *buffer, uint32_t length)
{
    if ((address + length) > TEST_FAKE_NVM_BYTES)
    {
        return 0U;
    }

    for (uint32_t i = 0; i < length; ++i)
    {
        buffer[i] = g_fake_nvm[address + i];
    }

    return length;
}

static uint32_t fake_nvm_write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if ((address + length) > TEST_FAKE_NVM_BYTES)
    {
        return 0U;
    }

    for (uint32_t i = 0; i < length; ++i)
    {
        g_fake_nvm[address + i] = buffer[i];
    }

    return length;
}

static void reset_fake_nvm(void)
{
    (void)memset(g_fake_nvm, 0xFFU, sizeof(g_fake_nvm));
}

void setUp(void)
{
    reset_fake_nvm();
}

void tearDown(void)
{
    reset_fake_nvm();
}

void test_init_empty_table(void)
{
    nvm_fault_logger_t logger;
    nvm_fault_logger_config_t config;
    nvm_fault_logger_status_t status;

    config.base_address = 0U;
    config.slot_count = 4U;
    config.slot_size = NVM_FAULT_LOGGER_RECORD_BYTES;
    config.read_fn = fake_nvm_read;
    config.write_fn = fake_nvm_write;

    status = nvm_fault_logger_init(&logger, &config);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(0U, nvm_fault_logger_get_valid_count(&logger));
    TEST_ASSERT_EQUAL_UINT32(0U, logger.newest_sequence);
    TEST_ASSERT_EQUAL_UINT32(1U, nvm_fault_logger_get_next_sequence(&logger));
    TEST_ASSERT_EQUAL_UINT32(0U, nvm_fault_logger_get_active_slot(&logger));
}

void test_log_sequential_faults_and_sequence_monotonicity(void)
{
    nvm_fault_logger_t logger;
    nvm_fault_logger_config_t config;
    nvm_fault_logger_status_t status;
    uint8_t payload[8U];
    nvm_fault_entry_t latest;

    config.base_address = 0U;
    config.slot_count = 4U;
    config.slot_size = NVM_FAULT_LOGGER_RECORD_BYTES;
    config.read_fn = fake_nvm_read;
    config.write_fn = fake_nvm_write;

    status = nvm_fault_logger_init(&logger, &config);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);

    for (uint32_t i = 0; i < 3U; ++i)
    {
        payload[0] = (uint8_t)(i + 1U);
        payload[1] = (uint8_t)(i + 10U);
        payload[2] = (uint8_t)(i + 20U);
        payload[3] = (uint8_t)(i + 30U);
        payload[4] = 0U;
        payload[5] = 0U;
        payload[6] = 0U;
        payload[7] = 0U;

        status = nvm_fault_logger_log(&logger, 1000U + (i * 50U), (uint16_t)(NVM_FAULT_E_STOP + i), payload);
        TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    }

    TEST_ASSERT_EQUAL_UINT32(3U, nvm_fault_logger_get_valid_count(&logger));
    TEST_ASSERT_EQUAL_UINT32(4U, nvm_fault_logger_get_next_sequence(&logger));

    status = nvm_fault_logger_get_latest(&logger, &latest);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(3U, latest.sequence);
    TEST_ASSERT_EQUAL_UINT32(1100U, latest.timestamp_ms);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_E_STOP + 2U, latest.fault_code);
    TEST_ASSERT_EQUAL_UINT8(3U, latest.payload[0]);
}

void test_wraparound_overwrites_oldest_slot_cleanly(void)
{
    nvm_fault_logger_t logger;
    nvm_fault_logger_config_t config;
    nvm_fault_logger_status_t status;
    uint8_t payload[8U];
    nvm_fault_entry_t latest;

    config.base_address = 0U;
    config.slot_count = 4U;
    config.slot_size = NVM_FAULT_LOGGER_RECORD_BYTES;
    config.read_fn = fake_nvm_read;
    config.write_fn = fake_nvm_write;

    status = nvm_fault_logger_init(&logger, &config);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);

    for (uint32_t i = 0; i < 6U; ++i)
    {
        payload[0] = (uint8_t)(i + 1U);
        payload[1] = (uint8_t)(i + 11U);
        payload[2] = (uint8_t)(i + 21U);
        payload[3] = (uint8_t)(i + 31U);
        payload[4] = (uint8_t)(i + 41U);
        payload[5] = (uint8_t)(i + 51U);
        payload[6] = (uint8_t)(i + 61U);
        payload[7] = (uint8_t)(i + 71U);

        status = nvm_fault_logger_log(&logger, 2000U + (i * 75U), (uint16_t)(NVM_FAULT_OVER_VOLTAGE + i), payload);
        TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    }

    TEST_ASSERT_EQUAL_UINT32(4U, nvm_fault_logger_get_valid_count(&logger));
    TEST_ASSERT_EQUAL_UINT32(7U, nvm_fault_logger_get_next_sequence(&logger));

    status = nvm_fault_logger_get_latest(&logger, &latest);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(6U, latest.sequence);
    TEST_ASSERT_EQUAL_UINT32(2375U, latest.timestamp_ms);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_OVER_VOLTAGE + 5U, latest.fault_code);
}

void test_corruption_recovery_skips_torn_or_bad_entries(void)
{
    nvm_fault_logger_t logger;
    nvm_fault_logger_config_t config;
    nvm_fault_logger_status_t status;
    uint8_t payload[8U];
    nvm_fault_entry_t latest;

    config.base_address = 0U;
    config.slot_count = 4U;
    config.slot_size = NVM_FAULT_LOGGER_RECORD_BYTES;
    config.read_fn = fake_nvm_read;
    config.write_fn = fake_nvm_write;

    status = nvm_fault_logger_init(&logger, &config);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);

    for (uint32_t i = 0; i < 2U; ++i)
    {
        payload[0] = (uint8_t)(i + 1U);
        payload[1] = (uint8_t)(i + 2U);
        payload[2] = (uint8_t)(i + 3U);
        payload[3] = (uint8_t)(i + 4U);
        payload[4] = (uint8_t)(i + 5U);
        payload[5] = (uint8_t)(i + 6U);
        payload[6] = (uint8_t)(i + 7U);
        payload[7] = (uint8_t)(i + 8U);

        status = nvm_fault_logger_log(&logger, 3000U + (i * 100U), (uint16_t)(NVM_FAULT_WATCHDOG_RESET + i), payload);
        TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    }

    TEST_ASSERT_EQUAL_UINT32(2U, nvm_fault_logger_get_valid_count(&logger));

    /* Corrupt the second slot (slot 1) by tampering with CRC */
    uint32_t slot_offset = NVM_FAULT_LOGGER_RECORD_BYTES * 1U;
    g_fake_nvm[slot_offset + 20U] ^= 0xFFU;

    /* Re-initialize logger: it should detect corrupted slot 1 and recover with only slot 0 */
    status = nvm_fault_logger_init(&logger, &config);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1U, nvm_fault_logger_get_valid_count(&logger));

    status = nvm_fault_logger_get_latest(&logger, &latest);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1U, latest.sequence);

    /* Log a new fault: it should cleanly overwrite the invalid slot */
    payload[0] = 0xAAU;
    status = nvm_fault_logger_log(&logger, 5000U, NVM_FAULT_THERMAL_RUNAWAY, payload);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(2U, nvm_fault_logger_get_valid_count(&logger));

    status = nvm_fault_logger_get_latest(&logger, &latest);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_LOGGER_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(2U, latest.sequence);
    TEST_ASSERT_EQUAL_UINT32(NVM_FAULT_THERMAL_RUNAWAY, latest.fault_code);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_empty_table);
    RUN_TEST(test_log_sequential_faults_and_sequence_monotonicity);
    RUN_TEST(test_wraparound_overwrites_oldest_slot_cleanly);
    RUN_TEST(test_corruption_recovery_skips_torn_or_bad_entries);

    return UNITY_END();
}
