#include "fiat_v0.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Fiat: KeeLoq-based rolling code protocol
// Similar structure to Kia but with different timing and key
// Uses Manchester encoding with 200us TE
// 60-bit packets typical

static bool fiat_decode_generic(const uint32_t* pulses, uint32_t num_pulses,
                                 DecodeResult* result, uint8_t variant) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 200;
    uint64_t frame = 0;
    uint32_t bit_count = 0;
    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;

    // Find sync (typically shorter for Fiat)
    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 8); i += 2) {
        if(i + 1 < num_pulses && pulses[i] > te * 3 && pulses[i + 1] > te * 3) {
            start = i + 2; break;
        }
    }

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 60; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 40) return false;

    // Fiat frame parsing varies by variant
    uint32_t serial;
    uint8_t button;
    uint16_t counter;
    uint32_t crc_val;

    if(variant == 0) {
        serial = (uint32_t)(frame & 0xFFFF) << 16 | (uint32_t)((frame >> 32) & 0xFFFF);
        button = (uint8_t)((frame >> 48) & 0x0F);
        counter = (uint16_t)((frame >> 20) & 0x0FFF);
        crc_val = (uint32_t)((frame >> 52) & 0xFF);
    } else if(variant == 1) {
        serial = (uint32_t)(frame & 0xFFFFFF);
        button = (uint8_t)((frame >> 24) & 0x0F);
        counter = (uint16_t)((frame >> 28) & 0xFFFF);
        crc_val = (uint32_t)((frame >> 44) & 0xFFFF);
    } else {
        serial = (uint32_t)(frame & 0xFFFFFFFF);
        button = (uint8_t)((frame >> 40) & 0x0F);
        counter = (uint16_t)((frame >> 44) & 0xFFF);
        crc_val = (uint32_t)((frame >> 56) & 0x0F);
    }

    result->serial = serial;
    result->button = button;
    result->counter = counter;
    result->crc = crc_val;
    result->valid = true;

    char name[16], display[32];
    snprintf(name, sizeof(name), "FiatV%u", variant);
    snprintf(display, sizeof(display), "Fiat V%u", variant);

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerFiat; pv.variant = variant;
    pv.name = name; pv.display_name = display;
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

bool fiat_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return fiat_decode_generic(pulses, num_pulses, result, 0);
}
bool fiat_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return fiat_decode_generic(pulses, num_pulses, result, 1);
}
bool fiat_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    return fiat_decode_generic(pulses, num_pulses, result, 2);
}
