#pragma once

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// Common types used across the application
#pragma pack(push, 1)

// Generic fixed code message
typedef struct {
    uint32_t serial;
    uint8_t button;
    uint16_t counter;
    uint16_t crc;
} GenericRemoteMessage;

// KeeLoq message structure
typedef struct {
    uint32_t serial;
    uint16_t counter;
    uint8_t button;
    uint16_t encrypted; // Encrypted portion
    uint16_t crc;
} KeeLoqMessage;

// PWM pulse (short=te, long=te*3)
typedef struct {
    uint32_t te; // Time element in microseconds
    uint32_t short_pulse;
    uint32_t long_pulse;
    uint32_t gap;
} PWMTiming;

// Common PWM timings
#define PWM_TE_SHORT 400   // us - short timing element
#define PWM_TE_LONG 1200   // us - long timing element
#define PWM_TE_MEDIUM 800  // us - medium timing element

// Manchester encoding timings
#define MANCHESTER_TE 200  // us
#define MANCHESTER_SHORT 200
#define MANCHESTER_LONG 400

// Standard KeeLoq timing
#define KEELOQ_TE 200
#define KEELOQ_SHORT_PULSE 200
#define KEELOQ_LONG_PULSE 400
#define KEELOQ_GAP 4000

// Standard gap between transmission repeats (us)
#define TX_GAP_SHORT 10000
#define TX_GAP_MEDIUM 20000
#define TX_GAP_LONG 40000

#pragma pack(pop)

// Decoding helper functions
uint32_t protopirate_reverse_bits(uint32_t val, uint8_t bits);
uint32_t protopirate_bytes_to_uint32(const uint8_t* data, uint8_t offset, uint8_t bits);
void protopirate_uint32_to_bytes(uint32_t val, uint8_t* data, uint8_t offset, uint8_t bits);

// Manchester helpers
bool protopirate_decode_manchester(const uint32_t* pulses, uint32_t num_pulses,
                                    uint32_t te, uint32_t* out_bits, uint32_t* out_bit_count);
uint32_t protopirate_encode_manchester(uint32_t data, uint32_t num_bits,
                                        uint32_t* pulses, uint32_t max_pulses, uint32_t te);

// PWM helpers
bool protopirate_decode_pwm(const uint32_t* pulses, uint32_t num_pulses,
                             uint32_t te_short, uint32_t te_long,
                             uint32_t* out_bits, uint32_t* out_bit_count);
uint32_t protopirate_encode_pwm(uint32_t data, uint32_t num_bits,
                                 uint32_t* pulses, uint32_t max_pulses,
                                 uint32_t te_short, uint32_t te_long, uint32_t gap);

// CRC calculations
uint16_t protopirate_crc16(const uint8_t* data, uint32_t len, uint16_t poly, uint16_t init);
uint8_t protopirate_crc8(const uint8_t* data, uint32_t len, uint8_t poly, uint8_t init);
uint32_t protopirate_keeloq_encrypt(uint32_t data, uint32_t key_l, uint32_t key_h);
uint32_t protopirate_keeloq_decrypt(uint32_t data, uint32_t key_l, uint32_t key_h);

// Protocol-specific CRC calculators
uint32_t protopirate_calc_keeloq_crc(uint32_t serial, uint32_t button, uint32_t counter);
uint32_t protopirate_calc_ford_crc(uint32_t serial, uint32_t button, uint32_t counter);
uint32_t protopirate_calc_honda_crc(uint32_t serial, uint32_t button, uint32_t counter);

// Waveform builders
uint32_t protopirate_build_keeloq_waveform(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);
uint32_t protopirate_build_fixed_waveform(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);
uint32_t protopirate_build_manchester_waveform(uint32_t data, uint32_t num_bits,
                                                uint32_t* pulses, uint32_t max_pulses, uint32_t te);

// Simple Manchester decode (2 pulses per bit)
#define MANCHESTER_BIT_0 0  // short high + short low
#define MANCHESTER_BIT_1 1  // short low + short high

// Timing analysis
uint32_t protopirate_find_short_pulse(const uint32_t* pulses, uint32_t num_pulses);
uint32_t protopirate_find_long_pulse(const uint32_t* pulses, uint32_t num_pulses);
uint32_t protopirate_calculate_te(const uint32_t* pulses, uint32_t num_pulses);
