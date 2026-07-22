#include "honda_v1.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Honda V1: Rolling code, 64-bit, PWM encoded
// Uses XOR-based checksum
// Sync + 64 PWM bits

static bool honda_generic_decode(const uint32_t* pulses, uint32_t num_pulses,
                                  DecodeResult* result, uint8_t variant) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te_short = 400;
    uint32_t te_long = 1200;
    uint64_t data = 0;
    uint32_t bit_count = 0;

    // Find sync
    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 3) { start = i + 1; break; }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 64; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else if(p > long_max + 2000) break;
    }

    if(bit_count < 30) return false;

    if(variant == 1) {
        // V1: [31:0] serial, [39:32] button, [55:40] counter, [63:56] checksum
        result->serial = (uint32_t)(data & 0xFFFFFFFF);
        result->button = (uint8_t)((data >> 32) & 0xFF);
        result->counter = (uint16_t)((data >> 40) & 0xFFFF);
        result->crc = (uint32_t)((data >> 56) & 0xFF);

        // Verify checksum: XOR of all bytes
        uint8_t xor_check = 0;
        for(uint32_t i = 0; i < 7; i++) {
            xor_check ^= (uint8_t)(data >> (i * 8));
        }
        if(xor_check != (result->crc & 0xFF)) return false;

    } else {
        // V2: [23:0] serial, [31:24] button, [47:32] counter, [63:48] checksum
        result->serial = (uint32_t)(data & 0xFFFFFF);
        result->button = (uint8_t)((data >> 24) & 0xFF);
        result->counter = (uint16_t)((data >> 32) & 0xFFFF);
        result->crc = (uint32_t)((data >> 48) & 0xFFFF);
    }

    result->valid = true;

    char name[16], display[32];
    snprintf(name, sizeof(name), "HondaV%u", variant);
    snprintf(display, sizeof(display), "Honda V%u", variant);

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerHonda; pv.variant = variant;
    pv.name = name; pv.display_name = display;
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

bool honda_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return honda_generic_decode(pulses, num_pulses, result, 1);
}

bool honda_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return honda_generic_decode(pulses, num_pulses, result, 2);
}

// Honda Static: Fixed code, simple 32-bit PWM
bool honda_static_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 10) return false;

    uint32_t te_short = 300;
    uint32_t te_long = 900;
    uint32_t data = 0;
    uint32_t bit_count = 0;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 2) { start = i + 1; break; }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 32; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else break;
    }

    if(bit_count < 14) return false;

    result->serial = data;
    result->fixed_code = data;
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerHonda; pv.variant = 0;
    pv.name = "HondaStatic"; pv.display_name = "Honda Static";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
