#include "star_line.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// StarLine: Russian alarm system, KeeLoq-based
// Uses 80-bit packets, Manchester encoded
// Contains serial, button, counter, CRC

bool star_line_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 26) return false;

    uint32_t te = 200;
    uint64_t frame_hi = 0;
    uint64_t frame_lo = 0;
    uint32_t bit_count = 0;

    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 8); i += 2) {
        if(i + 1 < num_pulses && pulses[i] > te * 4 && pulses[i + 1] > te * 4) {
            start = i + 2; break;
        }
    }

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 80; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            if(bit_count < 64) frame_lo = (frame_lo << 1) | 0;
            else frame_hi = (frame_hi << 1) | 0;
            bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            if(bit_count < 64) frame_lo = (frame_lo << 1) | 1;
            else frame_hi = (frame_hi << 1) | 1;
            bit_count++;
        } else break;
    }

    if(bit_count < 60) return false;

    // StarLine: 32-bit serial, 8-bit button, 16-bit counter, 24-bit CRC
    result->serial = (uint32_t)(frame_lo & 0xFFFFFFFF);
    result->button = (uint8_t)((frame_lo >> 32) & 0xFF);
    result->counter = (uint16_t)((frame_lo >> 40) & 0xFFFF);
    result->crc = (uint32_t)(frame_hi & 0xFFFFFF);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerStarLine; pv.variant = 0; pv.name = "StarLine";
    pv.display_name = "StarLine";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// Scher-Khan: Russian alarm, 64-bit KeeLoq variant
bool scher_khan_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 220;
    uint64_t frame = 0;
    uint32_t bit_count = 0;
    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 8); i += 2) {
        if(i + 1 < num_pulses && pulses[i] > te * 3 && pulses[i + 1] > te * 3) {
            start = i + 2; break;
        }
    }

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 64; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 48) return false;

    result->serial = (uint32_t)(frame & 0xFFFFFFFF);
    result->button = (uint8_t)((frame >> 48) & 0x0F);
    result->counter = (uint16_t)((frame >> 32) & 0xFFFF);
    result->crc = (uint32_t)((frame >> 52) & 0xFFF);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerScherKhan; pv.variant = 0; pv.name = "ScherKhan";
    pv.display_name = "Scher-Khan";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// Mitsubishi V0: Fixed code, 32-bit, simple PWM
bool mitsubishi_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 10) return false;

    uint32_t te_short = 350;
    uint32_t te_long = 1050;
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

    if(bit_count < 16) return false;

    result->serial = data;
    result->button = (data >> 24) & 0x0F;
    result->fixed_code = data;
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerMitsubishi; pv.variant = 0; pv.name = "MitsubishiV0";
    pv.display_name = "Mitsubishi V0";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// Mazda V0: Rolling code, 48-bit, Manchester encoded
bool mazda_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 16) return false;

    uint32_t te = 250;
    uint64_t frame = 0;
    uint32_t bit_count = 0;
    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 6); i += 2) {
        if(i + 1 < num_pulses && pulses[i] > te * 3) { start = i + 2; break; }
    }

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 48; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 30) return false;

    result->serial = (uint32_t)(frame & 0xFFFFFF);
    result->button = (uint8_t)((frame >> 24) & 0x0F);
    result->counter = (uint16_t)((frame >> 28) & 0xFFFF);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerMazda; pv.variant = 0; pv.name = "MazdaV0";
    pv.display_name = "Mazda V0";
    pv.features = PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
