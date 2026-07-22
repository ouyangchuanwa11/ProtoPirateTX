#include "subaru.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Subaru: Fixed code protocol, PWM encoded
// Uses tri-state bits: 0 = short, 1 = long, F = floating
// Packet is 40 bits with sync

bool subaru_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 14) return false;

    uint32_t te = 400;
    uint32_t te_short = te;
    uint32_t te_long = te * 3;
    uint32_t te_float = te * 7; // Floating bit

    uint32_t data = 0;
    uint32_t bit_count = 0;

    // Find sync
    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i++) {
        if(pulses[i] > te_long * 3) {
            start = i + 1;
            break;
        }
    }

    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;

    for(uint32_t i = start; i < num_pulses && bit_count < 40; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) {
            data = (data << 1) | 0;
            bit_count++;
        } else if(p >= long_min && p <= long_max) {
            data = (data << 1) | 1;
            bit_count++;
        } else if(p > te_float) {
            // Floating bit - treat as 1 in many fixed code systems
            data = (data << 1) | 1;
            bit_count++;
        } else {
            break;
        }
    }

    if(bit_count < 20) return false;

    // Subaru: [19:0] serial, [23:20] button, rest is fixed/checksum
    uint32_t serial = data & 0xFFFFF;
    uint8_t button = (data >> 20) & 0x0F;

    result->serial = serial;
    result->button = button;
    result->fixed_code = data;
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerSubaru; pv.variant = 0; pv.name = "Subaru";
    pv.display_name = "Subaru";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
