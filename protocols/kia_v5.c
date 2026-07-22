#include "kia_v5.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Kia V5: 66-bit KeeLoq with custom sync pattern
bool kia_v5_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;
    uint32_t te = 200;
    uint64_t frame = 0;
    if(!protopirate_decode_keeloq_frame(pulses, num_pulses, te, &frame)) return false;

    uint32_t serial; uint8_t button; uint16_t counter; uint32_t encrypted;
    protopirate_parse_keeloq_frame(frame, &serial, &button, &counter, &encrypted);
    if(serial == 0) return false;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = encrypted; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerKia; pv.variant = 5; pv.name = "KiaV5";
    pv.display_name = "Kia V5";
    pv.features = PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ;
    pv.packet_len_bits = 66;
    result->protocol = &pv;
    return true;
}

// Kia V6: 64-bit frame, different bit layout
bool kia_v6_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;
    uint32_t te = 220;
    uint64_t frame = 0;
    if(!protopirate_decode_keeloq_frame(pulses, num_pulses, te, &frame)) return false;

    // V6: [31:0] enc, [47:32] serial, [55:48] counter_low, [63:56] button+counter_high
    uint32_t encrypted = (uint32_t)(frame & 0xFFFFFFFF);
    uint32_t serial = (uint32_t)((frame >> 32) & 0xFFFF);
    uint8_t button = (uint8_t)((frame >> 60) & 0x0F);
    uint16_t counter = (uint16_t)((frame >> 48) & 0xFFF);

    if(serial == 0) return false;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = encrypted; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerKia; pv.variant = 6; pv.name = "KiaV6";
    pv.display_name = "Kia V6";
    pv.features = PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ;
    pv.packet_len_bits = 64;
    result->protocol = &pv;
    return true;
}

// Kia V7: 72-bit frame
bool kia_v7_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 24) return false;
    uint32_t te = 180;
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

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 72; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 64) return false;

    uint32_t serial = (uint32_t)(frame & 0xFFFFFFFF);
    uint8_t button = (uint8_t)((frame >> 56) & 0x0F);
    uint16_t counter = (uint16_t)((frame >> 40) & 0xFFFF);
    uint32_t crc_val = (uint32_t)((frame >> 32) & 0xFF);

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = crc_val; result->valid = true;
    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerKia; pv.variant = 7; pv.name = "KiaV7";
    pv.display_name = "Kia V7";
    pv.features = PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
