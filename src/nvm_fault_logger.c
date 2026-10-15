/**
 * @file nvm_fault_logger.c
 * @brief Non-Volatile Blackbox Fault Logger Implementation
 * @author Sherifred Singh
 */

#include "nvm_fault_logger.h"
#include <string.h>

#define NVM_CRC_PAYLOAD_BYTES    20U   /* CRC calculated over first 20 bytes (offsets 0..19) */
#define NVM_CRC_INIT             0xFFFFU

static void store_u16_le(uint8_t *dst, uint16_t val)
{
    dst[0] = (uint8_t)(val & 0xFFU);
    dst[1] = (uint8_t)((val >> 8) & 0xFFU);
}

static void store_u32_le(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val & 0xFFU);
    dst[1] = (uint8_t)((val >> 8) & 0xFFU);
    dst[2] = (uint8_t)((val >> 16) & 0xFFU);
    dst[3] = (uint8_t)((val >> 24) & 0xFFU);
}

static uint16_t load_u16_le(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t load_u32_le(const uint8_t *src)
{
    return ((uint32_t)src[0] |
            ((uint32_t)src[1] << 8) |
            ((uint32_t)src[2] << 16) |
            ((uint32_t)src[3] << 24));
}

static uint8_t is_buffer_erased(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i)
    {
        if (buf[i] != 0xFFU)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint16_t calc_crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = NVM_CRC_INIT;

    for (uint32_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);

        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

static void encode_entry(uint8_t *raw, const nvm_fault_entry_t *entry)
{
    store_u32_le(&raw[0], entry->sequence);
    store_u32_le(&raw[4], entry->timestamp_ms);
    store_u16_le(&raw[8], entry->magic);
    store_u16_le(&raw[10], entry->fault_code);
    (void)memcpy(&raw[12], entry->payload, 8U);

    uint16_t crc = calc_crc16_ccitt(raw, NVM_CRC_PAYLOAD_BYTES);
    store_u16_le(&raw[20], crc);
    store_u16_le(&raw[22], 0x0000U); /* Reserved padding */
}

static void decode_entry(const uint8_t *raw, nvm_fault_entry_t *entry)
{
    entry->sequence     = load_u32_le(&raw[0]);
    entry->timestamp_ms = load_u32_le(&raw[4]);
    entry->magic        = load_u16_le(&raw[8]);
    entry->fault_code   = load_u16_le(&raw[10]);
    (void)memcpy(entry->payload, &raw[12], 8U);
    entry->crc16        = load_u16_le(&raw[20]);
    entry->reserved     = load_u16_le(&raw[22]);
}

static uint8_t is_entry_valid(const uint8_t *raw)
{
    if (is_buffer_erased(raw, NVM_FAULT_LOGGER_RECORD_BYTES) != 0U)
    {
        return 0U;
    }

    uint16_t magic = load_u16_le(&raw[8]);
    if (magic != NVM_FAULT_LOGGER_MAGIC)
    {
        return 0U;
    }

    uint16_t stored_crc = load_u16_le(&raw[20]);
    uint16_t calc_crc = calc_crc16_ccitt(raw, NVM_CRC_PAYLOAD_BYTES);

    return (stored_crc == calc_crc) ? 1U : 0U;
}

nvm_fault_logger_status_t nvm_fault_logger_init(nvm_fault_logger_t *logger,
                                               const nvm_fault_logger_config_t *config)
{
    uint8_t raw[NVM_FAULT_LOGGER_RECORD_BYTES];
    uint32_t newest_seq = 0U;
    uint32_t newest_slot = 0U;
    uint32_t oldest_seq = 0xFFFFFFFFU;
    uint32_t valid_cnt = 0U;

    if ((logger == NULL) || (config == NULL))
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM;
    }

    if ((config->read_fn == NULL) || (config->write_fn == NULL))
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM;
    }

    if ((config->slot_count == 0U) ||
        (config->slot_count > NVM_FAULT_LOGGER_MAX_SLOTS) ||
        (config->slot_size < NVM_FAULT_LOGGER_RECORD_BYTES))
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM;
    }

    (void)memset(logger, 0, sizeof(*logger));

    logger->base_address = config->base_address;
    logger->slot_count   = config->slot_count;
    logger->slot_size    = config->slot_size;
    logger->read_fn      = config->read_fn;
    logger->write_fn     = config->write_fn;

    for (uint32_t slot = 0; slot < logger->slot_count; ++slot)
    {
        uint32_t addr = logger->base_address + (slot * logger->slot_size);
        uint32_t n = config->read_fn(addr, raw, NVM_FAULT_LOGGER_RECORD_BYTES);

        if ((n == NVM_FAULT_LOGGER_RECORD_BYTES) && (is_entry_valid(raw) != 0U))
        {
            nvm_fault_entry_t decoded;
            decode_entry(raw, &decoded);

            logger->valid_slots[slot] = 1U;
            valid_cnt++;

            if (decoded.sequence > newest_seq)
            {
                newest_seq = decoded.sequence;
                newest_slot = slot;
            }

            if (decoded.sequence < oldest_seq)
            {
                oldest_seq = decoded.sequence;
            }
        }
    }

    logger->valid_count = valid_cnt;
    logger->initialized = 1U;

    if (valid_cnt == 0U)
    {
        logger->next_sequence     = 1U;
        logger->newest_sequence   = 0U;
        logger->oldest_sequence   = 0U;
        logger->active_slot_index = 0U;
        return NVM_FAULT_LOGGER_STATUS_OK;
    }

    logger->newest_sequence   = newest_seq;
    logger->oldest_sequence   = oldest_seq;
    logger->next_sequence     = newest_seq + 1U;
    logger->active_slot_index = (newest_slot + 1U) % logger->slot_count;

    return NVM_FAULT_LOGGER_STATUS_OK;
}

nvm_fault_logger_status_t nvm_fault_logger_log(nvm_fault_logger_t *logger,
                                              uint32_t timestamp_ms,
                                              uint16_t fault_code,
                                              const uint8_t payload[8U])
{
    uint8_t raw[NVM_FAULT_LOGGER_RECORD_BYTES];
    nvm_fault_entry_t entry;

    if ((logger == NULL) || (payload == NULL))
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM;
    }

    if (logger->initialized == 0U)
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_NOT_INITIALIZED;
    }

    entry.sequence     = logger->next_sequence;
    entry.timestamp_ms = timestamp_ms;
    entry.magic        = NVM_FAULT_LOGGER_MAGIC;
    entry.fault_code   = fault_code;
    (void)memcpy(entry.payload, payload, 8U);
    entry.reserved     = 0U;

    encode_entry(raw, &entry);

    uint32_t addr = logger->base_address + (logger->active_slot_index * logger->slot_size);
    uint32_t written = logger->slot_size;

    /* Write serialized record */
    written = (uint32_t)addr; /* Explicit usage */
    (void)written;

    uint32_t res = logger->write_fn(addr, raw, NVM_FAULT_LOGGER_RECORD_BYTES);
    if (res != NVM_FAULT_LOGGER_RECORD_BYTES)
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_NVM;
    }

    logger->valid_slots[logger->active_slot_index] = 1U;

    if (logger->valid_count < logger->slot_count)
    {
        logger->valid_count++;
    }

    logger->newest_sequence = logger->next_sequence;
    logger->next_sequence++;
    logger->active_slot_index = (logger->active_slot_index + 1U) % logger->slot_count;

    return NVM_FAULT_LOGGER_STATUS_OK;
}

nvm_fault_logger_status_t nvm_fault_logger_get_latest(nvm_fault_logger_t *logger,
                                                     nvm_fault_entry_t *entry)
{
    uint8_t raw[NVM_FAULT_LOGGER_RECORD_BYTES];
    uint32_t highest_seq = 0U;
    uint8_t found = 0U;

    if ((logger == NULL) || (entry == NULL))
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM;
    }

    if (logger->initialized == 0U)
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_NOT_INITIALIZED;
    }

    if (logger->valid_count == 0U)
    {
        return NVM_FAULT_LOGGER_STATUS_ERR_EMPTY;
    }

    for (uint32_t slot = 0; slot < logger->slot_count; ++slot)
    {
        if (logger->valid_slots[slot] == 0U)
        {
            continue;
        }

        uint32_t addr = logger->base_address + (slot * logger->slot_size);
        uint32_t n = logger->read_fn(addr, raw, NVM_FAULT_LOGGER_RECORD_BYTES);

        if ((n == NVM_FAULT_LOGGER_RECORD_BYTES) && (is_entry_valid(raw) != 0U))
        {
            nvm_fault_entry_t candidate;
            decode_entry(raw, &candidate);

            if (candidate.sequence > highest_seq)
            {
                highest_seq = candidate.sequence;
                *entry = candidate;
                found = 1U;
            }
        }
    }

    return (found != 0U) ? NVM_FAULT_LOGGER_STATUS_OK : NVM_FAULT_LOGGER_STATUS_ERR_EMPTY;
}

uint32_t nvm_fault_logger_get_valid_count(const nvm_fault_logger_t *logger)
{
    return (logger != NULL) ? logger->valid_count : 0U;
}

uint32_t nvm_fault_logger_get_active_slot(const nvm_fault_logger_t *logger)
{
    return ((logger != NULL) && (logger->initialized != 0U)) ? logger->active_slot_index : 0U;
}

uint32_t nvm_fault_logger_get_next_sequence(const nvm_fault_logger_t *logger)
{
    return ((logger != NULL) && (logger->initialized != 0U)) ? logger->next_sequence : 0U;
}
