#include "ford_v1.h"
#include "ford_v0.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Ford V1-V3 use the same physical layer as V0 but with different packet layouts
// They all share the PWM encoding with sync pulse

// Forward declare the generic decoder from ford_v0.c
// (it's actually defined in ford_v0.c so we re-import the logic here)

bool ford_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    // Ford V1: Uses the same decoding as V0 but different data extraction
    if(!pulses || !result || num_pulses < 16) return false;

    uint32_t te_short = 300;
    uint32_t te_long = 900;
    uint32_t data = 0;
    uint32_t bit_count = 0;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 3) { start = i + 1; break; }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 48; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else if(p > long_max + 1000) break;
    }

    if(bit_count < 24) return false;

    // V1: 24-bit serial, 8-bit button, 8-bit counter, 8-bit checksum
    uint32_t serial = data & 0xFFFFFF;
    uint8_t button = (data >> 24) & 0xFF;
    uint16_t counter = (data >> 32) & 0xFF;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = (data >> 40) & 0xFF; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerFord; pv.variant = 1; pv.name = "FordV1";
    pv.display_name = "Ford V1";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

bool ford_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 16) return false;

    uint32_t te_short = 300;
    uint32_t te_long = 900;
    uint32_t data = 0;
    uint32_t bit_count = 0;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 3) { start = i + 1; break; }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 48; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else if(p > long_max + 1000) break;
    }

    if(bit_count < 24) return false;

    // V2: 32-bit serial (16+16), 4-bit button, 8-bit counter, 4-bit checksum
    uint32_t serial = ((data >> 16) & 0xFFFF) << 16 | (data & 0xFFFF);
    uint8_t button = (data >> 32) & 0x0F;
    uint16_t counter = (data >> 36) & 0xFF;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = (data >> 44) & 0x0F; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerFord; pv.variant = 2; pv.name = "FordV2";
    pv.display_name = "Ford V2";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

bool ford_v3_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 16) return false;

    uint32_t te_short = 300;
    uint32_t te_long = 900;
    uint32_t data = 0;
    uint32_t bit_count = 0;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 3) { start = i + 1; break; }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 48; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else if(p > long_max + 1000) break;
    }

    if(bit_count < 24) return false;

    // V3: 24-bit serial, 4-bit button, 16-bit counter, 4-bit checksum
    uint32_t serial = data & 0xFFFFFF;
    uint8_t button = (data >> 24) & 0x0F;
    uint16_t counter = (data >> 28) & 0xFFFF;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = (data >> 44) & 0x0F; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerFord; pv.variant = 3; pv.name = "FordV3";
    pv.display_name = "Ford V3";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
