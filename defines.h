#pragma once

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// Protocol type identifiers
typedef enum {
    ProtocolTypeNone = 0,
    // KeeLoq-based rolling code protocols
    ProtocolTypeKeeLoq,
    // Fixed code protocols
    ProtocolTypeFixed,
    // Other rolling code
    ProtocolTypeOther,
} ProtocolType;

// Manufacturer IDs
typedef enum {
    ManufacturerNone = 0,
    ManufacturerKia,
    ManufacturerFord,
    ManufacturerSubaru,
    ManufacturerFiat,
    ManufacturerChrysler,
    ManufacturerMitsubishi,
    ManufacturerMazda,
    ManufacturerHonda,
    ManufacturerStarLine,
    ManufacturerScherKhan,
    ManufacturerPSA,
    ManufacturerRenault,
    ManufacturerVAG,
    ManufacturerPorsche,
    ManufacturerAUT64,
    ManufacturerToyota,
    ManufacturerNissan,
    ManufacturerHyundai,
} Manufacturer;

// Feature flags
#define PROTO_HAS_TX (1 << 0)
#define PROTO_HAS_ROLLBACK (1 << 1)
#define PROTO_HAS_CRC (1 << 2)
#define PROTO_HAS_COUNTER (1 << 3)
#define PROTO_HAS_SERIAL (1 << 4)
#define PROTO_HAS_BUTTON (1 << 5)
#define PROTO_HAS_FIXED (1 << 6)
#define PROTO_KEE_LOQ (1 << 7)

// Protocol version
typedef struct {
    Manufacturer manufacturer;
    uint8_t variant;
    const char* name;
    const char* display_name;
    uint16_t features;
    uint8_t packet_len_bits;
} ProtocolVersion;

// Decoded signal info
typedef struct {
    uint32_t frequency;
    uint32_t serial;
    uint32_t button;
    uint32_t counter;
    uint32_t crc;
    uint32_t fixed_code;
    uint8_t* raw_data;
    uint32_t raw_len;
    uint32_t raw_len_bits;
    bool valid;
    ProtocolVersion* protocol;
} DecodeResult;

// Timing modulation types
typedef enum {
    ModulationPWM,
    ModulationManchester,
    ModulationASK,
    ModulationFSK,
} ModulationType;

// Decode state
typedef struct {
    uint32_t* pulses;
    uint32_t num_pulses;
    uint32_t sample_rate;
    bool decoded;
    DecodeResult result;
} DecodeState;

// View IDs for custom views
typedef enum {
    ViewIdNone = 0,
    ViewIdReceiver,
    ViewIdReceiverInfo,
    ViewIdSubmenu,
    ViewIdWidget,
    ViewIdPopup,
    ViewIdTextInput,
    ViewIdVariableItemList,
    ViewIdEmulate,
} ViewId;

// Config
#define PROTOPIRATE_DEFAULT_FREQ 433920000
#define PROTOPIRATE_HISTORY_MAX 50
#define PROTOPIRATE_MAX_PROTOCOLS 30
#define PROTOPIRATE_BUFFER_SIZE 8192
#define PROTOPIRATE_MAX_PULSES 4096

// Enable timing tuner scene
#define ENABLE_TIMING_TUNER_SCENE

// Frequency bands
typedef enum {
    FrequencyBand300, // 300-348 MHz
    FrequencyBand387, // 387-464 MHz
    FrequencyBand779, // 779-928 MHz
    FrequencyBandCount,
} FrequencyBand;
