#include "ford_v0.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Ford V0: PWM encoded, 48-bit packet
// Uses Manchester-like encoding with specific timing
// Sync + 48 data bits + guard

static bool ford_decode_generic(const uint32_t* pulses, uint32_t num_pulses,
                                  DecodeResult* result, uint8_t variant) {
    if(!pulses || !result || num_pulses < 16) return false;

    uint32_t te_short = 300;
    uint32_t te_long = 900;

    uint32_t data = 0;
    uint32_t bit_count = 0;

    // Ford uses PWM: short=0, long=1
    // Find sync first
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

    for(uint32_t i = start; i < num_pulses && bit_count < 48; i++) {
        uint32_t p = pulses[i];
        if(p >= short_min && p <= short_max) {
            data = (data << 1) | 0;
            bit_count++;
        } else if(p >= long_min && p <= long_max) {
            data = (data << 1) | 1;
            bit_count++;
        } else if(p > long_max + 1000) {
            break; // gap
        }
    }

    if(bit_count < 24) return false;

    // Ford packet layout (varies by variant):
    uint32_t serial = 0;
    uint8_t button = 0;
    uint16_t counter = 0;

    if(variant == 0) {
        // V0: [31:0] serial, [39:32] button, [47:40] checksum
        serial = data & 0xFFFFFFFF;
        button = (data >> 32) & 0xFF;
    } else if(variant == 1) {
        // V1: [23:0] serial, [31:24] button, [39:32] counter, [47:40] checksum
        serial = data & 0xFFFFFF;
        button = (data >> 24) & 0xFF;
        counter = (data >> 32) & 0xFF;
    } else if(variant == 2) {
        // V2: [15:0] serial_low, [31:16] serial_high, [35:32] button, [43:36] counter, [47:44] checksum
        serial = ((data >> 16) & 0xFFFF) << 16 | (data & 0xFFFF);
        button = (data >> 32) & 0x0F;
        counter = (data >> 36) & 0xFF;
    } else {
        // V3: [23:0] serial, [27:24] button, [43:28] counter, [47:44] checksum
        serial = data & 0xFFFFFF;
        button = (data >> 24) & 0x0F;
        counter = (data >> 28) & 0xFFFF;
    }

    result->serial = serial;
    result->button = button;
    result->counter = counter;
    result->crc = (data >> 40) & 0xFF;
    result->valid = true;

    char name[16], display[32];
    snprintf(name, sizeof(name), "FordV%u", variant);
    snprintf(display, sizeof(display), "Ford V%u", variant);

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerFord;
    pv.variant = variant;
    pv.name = name;
    pv.display_name = display;
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;

    return true;
}

bool ford_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return ford_decode_generic(pulses, num_pulses, result, 0);
}
