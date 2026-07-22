#pragma once

#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// Max protocol count
#define PROTOCOL_MAX_COUNT 30

// Protocol decoder function signature
typedef bool (*ProtocolDecodeFunc)(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Protocol build function signature (for TX)
typedef uint32_t (*ProtocolBuildFunc)(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);

// Protocol entry
typedef struct {
    ProtocolVersion version;
    ProtocolDecodeFunc decode;
    ProtocolBuildFunc build; // NULL if no TX
} ProtocolEntry;

// Global protocol list
extern ProtocolEntry* g_protocols[PROTOCOL_MAX_COUNT];
extern uint32_t g_protocol_count;

// Initialize all protocols
void protopirate_protocols_init(void);

// Find protocol by name
ProtocolVersion* protopirate_find_protocol_by_name(const char* name);

// Try all decoders on pulse data
bool protopirate_try_decode_all(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Get protocol at index
ProtocolEntry* protopirate_get_protocol(uint32_t index);
uint32_t protopirate_get_protocol_count(void);

// Forward declarations for all protocol decoder functions
// Kia
bool kia_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v3_v4_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v5_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v6_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v7_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Ford
bool ford_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool ford_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool ford_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool ford_v3_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Subaru
bool subaru_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Fiat
bool fiat_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool fiat_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool fiat_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Chrysler
bool chrysler_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// StarLine
bool star_line_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Scher-Khan
bool scher_khan_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Mitsubishi
bool mitsubishi_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Mazda
bool mazda_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Honda
bool honda_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool honda_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool honda_static_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// PSA
bool psa_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Porsche Touareg
bool porsche_touareg_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Renault
bool renault_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// VAG
bool vag_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// AUT64
bool aut64_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);

// Build functions (TX)
uint32_t kia_v0_build(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);
uint32_t kia_v1_build(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);
