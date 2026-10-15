/**
 * @file nvm_fault_logger.h
 * @brief Non-Volatile Blackbox Fault & Event Logger for Embedded Systems
 * @author Sherifred Singh
 *
 * Implements a wear-leveled, anti-tearing circular slot event recorder for
 * EEPROM / Flash emulation with power-loss recovery and CRC-16-CCITT integrity.
 */

#ifndef NVM_FAULT_LOGGER_H
#define NVM_FAULT_LOGGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NVM_FAULT_LOGGER_MAGIC          ((uint16_t)0xA55AU)
#define NVM_FAULT_LOGGER_MAX_SLOTS      128U
#define NVM_FAULT_LOGGER_RECORD_BYTES   24U  /* Serialized 24-byte record (4-byte aligned) */

typedef enum
{
    NVM_FAULT_LOGGER_STATUS_OK = 0U,
    NVM_FAULT_LOGGER_STATUS_ERR_INVALID_PARAM = 1U,
    NVM_FAULT_LOGGER_STATUS_ERR_NOT_INITIALIZED = 2U,
    NVM_FAULT_LOGGER_STATUS_ERR_NVM = 3U,
    NVM_FAULT_LOGGER_STATUS_ERR_FULL = 4U,
    NVM_FAULT_LOGGER_STATUS_ERR_CORRUPT = 5U,
    NVM_FAULT_LOGGER_STATUS_ERR_EMPTY = 6U
} nvm_fault_logger_status_t;

typedef enum
{
    NVM_FAULT_E_STOP          = 0x0001U,
    NVM_FAULT_OVER_VOLTAGE    = 0x0002U,
    NVM_FAULT_THERMAL_RUNAWAY = 0x0003U,
    NVM_FAULT_WATCHDOG_RESET  = 0x0004U,
    NVM_FAULT_UNDER_VOLTAGE   = 0x0005U,
    NVM_FAULT_CAN_BUS_OFF     = 0x0006U,
    NVM_FAULT_GROUND_FAULT    = 0x0007U
} nvm_fault_code_t;

/**
 * @brief In-memory fault record representation.
 * Explicitly aligned to 4-byte boundaries (24 bytes total).
 */
typedef struct
{
    uint32_t sequence;       /**< Monotonic sequence counter */
    uint32_t timestamp_ms;   /**< Uptime in milliseconds when fault triggered */
    uint16_t magic;          /**< 0xA55A Header Magic */
    uint16_t fault_code;     /**< Fault identifier enum */
    uint8_t  payload[8U];    /**< Diagnostic telemetry snapshot */
    uint16_t crc16;          /**< CRC-16-CCITT checksum */
    uint16_t reserved;       /**< 4-byte padding & future flags */
} nvm_fault_entry_t;

/**
 * @brief NVM hardware read callback function pointer.
 */
typedef uint32_t (*nvm_fault_logger_read_fn)(uint32_t address, uint8_t *buffer, uint32_t length);

/**
 * @brief NVM hardware write callback function pointer.
 */
typedef uint32_t (*nvm_fault_logger_write_fn)(uint32_t address, const uint8_t *buffer, uint32_t length);

/**
 * @brief Logger runtime hardware configuration.
 */
typedef struct
{
    uint32_t base_address;
    uint32_t slot_count;
    uint32_t slot_size;
    nvm_fault_logger_read_fn read_fn;
    nvm_fault_logger_write_fn write_fn;
} nvm_fault_logger_config_t;

/**
 * @brief Logger state context instance.
 */
typedef struct
{
    uint32_t base_address;
    uint32_t slot_count;
    uint32_t slot_size;
    uint32_t valid_count;
    uint32_t active_slot_index;
    uint32_t next_sequence;
    uint32_t newest_sequence;
    uint32_t oldest_sequence;
    nvm_fault_logger_read_fn read_fn;
    nvm_fault_logger_write_fn write_fn;
    uint8_t  initialized;
    uint8_t  valid_slots[NVM_FAULT_LOGGER_MAX_SLOTS];
} nvm_fault_logger_t;

/**
 * @brief Initializes the fault logger and scans the NVM region for valid entries.
 */
nvm_fault_logger_status_t nvm_fault_logger_init(nvm_fault_logger_t *logger,
                                               const nvm_fault_logger_config_t *config);

/**
 * @brief Appends a fault event to the next circular NVM slot.
 */
nvm_fault_logger_status_t nvm_fault_logger_log(nvm_fault_logger_t *logger,
                                              uint32_t timestamp_ms,
                                              uint16_t fault_code,
                                              const uint8_t payload[8U]);

/**
 * @brief Fetches the latest valid logged record.
 */
nvm_fault_logger_status_t nvm_fault_logger_get_latest(nvm_fault_logger_t *logger,
                                                     nvm_fault_entry_t *entry);

uint32_t nvm_fault_logger_get_valid_count(const nvm_fault_logger_t *logger);
uint32_t nvm_fault_logger_get_active_slot(const nvm_fault_logger_t *logger);
uint32_t nvm_fault_logger_get_next_sequence(const nvm_fault_logger_t *logger);

#ifdef __cplusplus
}
#endif

#endif /* NVM_FAULT_LOGGER_H */
