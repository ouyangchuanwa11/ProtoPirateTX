#include "chrysler_v0.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Chrysler V0: Fixed code protocol
// PWM with tri-state bits
// 40-bit packets

bool chrysler_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 14) return false;

    uint32_t te = 350;
    uint32_t data = 0;
    uint32_t bit_count = 0;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te * 5) { start = i + 1; break; }
    }

    uint32_t short_min = te * 60 / 100;
    uint32_t short_max = te * 160 / 100;
    uint32_t long_min = te * 3 * 60 / 100;
    uint32_t long_max = te * 3 * 160 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 40; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) { data = (data << 1) | 0; bit_count++; }
        else if(p >= long_min && p <= long_max) { data = (data << 1) | 1; bit_count++; }
        else if(p > long_max + 2000) break;
        else break;
    }

    if(bit_count < 20) return false;

    // Chrysler: [19:0] serial, [23:20] button, rest fixed
    result->serial = data & 0xFFFFF;
    result->button = (data >> 20) & 0x0F;
    result->fixed_code = data;
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerChrysler; pv.variant = 0; pv.name = "ChryslerV0";
    pv.display_name = "Chrysler V0";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
