#include "psa.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// PSA (Peugeot/Citroen): Rolling code, 56-bit, Manchester encoded
bool psa_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 18) return false;

    uint32_t te = 200;
    uint64_t frame = 0;
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

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 56; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 40) return false;

    // PSA: [31:0] serial, [39:32] button, [51:40] counter, [55:52] checksum
    result->serial = (uint32_t)(frame & 0xFFFFFFFF);
    result->button = (uint8_t)((frame >> 32) & 0xFF);
    result->counter = (uint16_t)((frame >> 40) & 0xFFF);
    result->crc = (uint32_t)((frame >> 52) & 0x0F);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerPSA; pv.variant = 0; pv.name = "PSA";
    pv.display_name = "PSA (Peugeot/Citroen)";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// Porsche Touareg: 60-bit, KeeLoq variant
bool porsche_touareg_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 200;

    // Try standard KeeLoq frame decode
    uint64_t frame = 0;
    if(!protopirate_decode_keeloq_frame(pulses, num_pulses, te, &frame)) {
        return false;
    }

    uint32_t serial; uint8_t button; uint16_t counter; uint32_t encrypted;
    protopirate_parse_keeloq_frame(frame, &serial, &button, &counter, &encrypted);
    if(serial == 0) return false;

    result->serial = serial; result->button = button;
    result->counter = counter; result->crc = encrypted; result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerPorsche; pv.variant = 0; pv.name = "PorscheTouareg";
    pv.display_name = "Porsche Touareg";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = 66;
    result->protocol = &pv;
    return true;
}

// Renault V0: 56-bit, Manchester encoded
bool renault_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 18) return false;

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

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 56; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 40) return false;

    // Renault: [31:0] serial, [35:32] button, [51:36] counter, [55:52] checksum
    result->serial = (uint32_t)(frame & 0xFFFFFFFF);
    result->button = (uint8_t)((frame >> 32) & 0x0F);
    result->counter = (uint16_t)((frame >> 36) & 0xFFFF);
    result->crc = (uint32_t)((frame >> 52) & 0x0F);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerRenault; pv.variant = 0; pv.name = "RenaultV0";
    pv.display_name = "Renault V0";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// VAG (VW/Audi/Seat/Skoda): 64-bit, Manchester encoded, KeeLoq-based
bool vag_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 200;
    uint64_t frame = 0;
    uint32_t bit_count = 0;
    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;
    uint32_t te4_min = te * 4 * 60 / 100;
    uint32_t te4_max = te * 4 * 160 / 100;

    uint32_t start = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 8); i += 2) {
        if(i + 1 < num_pulses && pulses[i] >= te4_min && pulses[i] <= te4_max &&
           pulses[i + 1] >= te4_min && pulses[i + 1] <= te4_max) {
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

    // VAG: [31:0] encrypted/serial, [47:32] counter, [55:48] button, [63:56] checksum
    result->serial = (uint32_t)(frame & 0xFFFFFFFF);
    result->button = (uint8_t)((frame >> 48) & 0xFF);
    result->counter = (uint16_t)((frame >> 32) & 0xFFFF);
    result->crc = (uint32_t)((frame >> 56) & 0xFF);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerVAG; pv.variant = 0; pv.name = "VAG";
    pv.display_name = "VAG (VW/Audi/Seat/Skoda)";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}

// AUT64: 56-bit, KeeLoq variant
bool aut64_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 18) return false;

    uint32_t te = 200;
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

    for(uint32_t i = start; i < num_pulses - 1 && bit_count < 56; i += 2) {
        uint32_t p1 = pulses[i]; uint32_t p2 = pulses[i + 1];
        if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
            frame = (frame << 1) | 0; bit_count++;
        } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
            frame = (frame << 1) | 1; bit_count++;
        } else break;
    }

    if(bit_count < 40) return false;

    // AUT64: [31:0] serial, [35:32] button, [51:36] counter, [55:52] CRC
    result->serial = (uint32_t)(frame & 0xFFFFFFFF);
    result->button = (uint8_t)((frame >> 32) & 0x0F);
    result->counter = (uint16_t)((frame >> 36) & 0xFFFF);
    result->crc = (uint32_t)((frame >> 52) & 0x0F);
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerAUT64; pv.variant = 0; pv.name = "AUT64";
    pv.display_name = "AUT64";
    pv.features = PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC;
    pv.packet_len_bits = (uint8_t)bit_count;
    result->protocol = &pv;
    return true;
}
