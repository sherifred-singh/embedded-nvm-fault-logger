# Embedded Non-Volatile Blackbox Fault & Event Logger (`nvm_fault_logger`)

[![NVM Logger CI](https://github.com/sherifred-singh/embedded-nvm-fault-logger/actions/workflows/ci.yml/badge.svg)](https://github.com/sherifred-singh/embedded-nvm-fault-logger/actions/workflows/ci.yml)
[![Language](https://img.shields.io/badge/Language-C99-blue.svg?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Compliance](https://img.shields.io/badge/Compliance-MISRA--C:2012-brightgreen.svg?style=flat-square)](https://www.misra.org.uk/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)

A high-reliability, hardware-independent **Non-Volatile Blackbox Fault Logger** written in strict **C99** for safety-critical automotive ECUs, medical devices, and industrial automation controllers (e.g. powerloom shedding systems, ozone disinfection machines).

---

## 📌 Architecture & Design Principles

```
┌────────────────────────────────────────────────────────────────────────┐
│               CIRCULAR WEAR-LEVELED NVM STORAGE MAP                    │
├──────────────┬──────────────┬──────────────┬──────────────┬────────────┤
│ Slot 0 (24B) │ Slot 1 (24B) │ Slot 2 (24B) │ Slot 3 (24B) │ ... Slot N │
│ [Seq: 101]   │ [Seq: 102]   │ [Seq: 103]   │ [Seq: 104]   │            │
│ [CRC: Valid] │ [CRC: Valid] │ [CRC: TORN!] │ [CRC: Valid] │            │
└──────────────┴──────────────┴──────────────┴──────────────┴────────────┘
                                      ▲
                                      └── Power-Fail Partial Write Discarded
```

### 1. Power-Fail Safe & Anti-Tearing
In industrial sheds and automotive environments, DC power can collapse unexpectedly mid-write. Every 24-byte record incorporates:
* **Magic Header (`0xA55A`):** Immediate fast-rejection of erased (`0xFF`) or unprogrammed flash blocks.
* **Monotonic 32-Bit Sequence Number:** Enables bootloader/recovery state machines to unambiguously order records without relying on real-time clocks.
* **CRC-16-CCITT Integrity Check:** Verified over payload and header. Corrupted or partially-written torn records are automatically detected and safely skipped during bootup.

### 2. Zero-Heap & MISRA-C Compliance
* **Zero Dynamic Allocation:** Absolutely no `malloc()`, `free()`, or heap fragmentation risks.
* **Deterministic Execution:** Fixed slot sizing and bounded $O(N)$ recovery scan.

---

## 📊 Serialized Record Layout (24 Bytes)

| Offset | Field | Type | Description |
|:---:|:---|:---:|:---|
| `0..3` | `sequence` | `uint32_t` | Monotonic counter incremented on every valid write |
| `4..7` | `timestamp_ms` | `uint32_t` | System uptime in milliseconds |
| `8..9` | `magic` | `uint16_t` | Constant `0xA55A` validation marker |
| `10..11`| `fault_code` | `uint16_t` | Application-specific fault code enum |
| `12..19`| `payload` | `uint8_t[8]` | Diagnostic snapshot (voltage, temperature, motor RPM) |
| `20..21`| `crc16` | `uint16_t` | CRC-16-CCITT checksum over bytes 0..19 |
| `22..23`| `reserved` | `uint16_t` | 4-byte alignment padding & future flags |

---

## 🧪 Automated Unit Test Suite (4/4 Passing)

The test suite validates boot recovery, monotonicity, circular wraparound, and power-fail corruption:
```bash
gcc -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -Itests/unity src/nvm_fault_logger.c tests/test_nvm_fault_logger.c tests/unity/unity.c -o test_runner
./test_runner
```

```text
-----------------------
4 Tests 0 Failures 0 Ignored 
OK
```

---

## 📄 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.  
Designed and engineered by **Sherifred Singh**.
