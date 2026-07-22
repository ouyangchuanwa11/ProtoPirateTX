#pragma once

#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// Common protocol decoding/encoding utilities for KeeLoq-based protocols
// These are protocol-agnostic helpers used by multiple protocol implementations

// Pulse analysis utilities
bool protopirate_analyze_pulses(const uint32_t* pulses, uint32_t num_pulses,
                                 uint32_t* te, uint32_t* short_pulse, uint32_t* long_pulse);
void protopirate_find_pulse_stats(const uint32_t* pulses, uint32_t num_pulses,
                                   uint32_t* min, uint32_t* max, uint32_t* avg);

// Decode KeeLoq 66-bit frame from Manchester-encoded pulses
bool protopirate_decode_keeloq_frame(const uint32_t* pulses, uint32_t num_pulses,
                                      uint32_t te, uint64_t* frame_out);

// Extract fields from a KeeLoq frame
// Standard KeeLoq 66-bit frame: [31:0] encrypted, [47:32] serial_high, [59:48] serial_low, [61:60] button, [63:62] ?
void protopirate_parse_keeloq_frame(uint64_t frame, uint32_t* serial,
                                     uint8_t* button, uint16_t* counter,
                                     uint32_t* encrypted);

// Calculate KeeLoq key from serial for a given manufacturer
bool protopirate_get_keeloq_key(uint32_t serial, Manufacturer manufacturer,
                                 uint32_t* key_l, uint32_t* key_h);

// Verify KeeLoq encrypted portion against decrypted data
bool protopirate_verify_keeloq(uint32_t encrypted, uint32_t serial,
                                uint8_t button, uint16_t counter,
                                uint32_t key_l, uint32_t key_h);

// Decode generic PWM data with sync detection
bool protopirate_decode_pwm_with_sync(const uint32_t* pulses, uint32_t num_pulses,
                                       uint32_t* data, uint32_t* bit_count,
                                       uint32_t te_short, uint32_t te_long);

// Decode Manchester with sync (sync pulse + data)
bool protopirate_decode_manchester_with_sync(const uint32_t* pulses, uint32_t num_pulses,
                                              uint32_t* data, uint32_t* bit_count,
                                              uint32_t te);
