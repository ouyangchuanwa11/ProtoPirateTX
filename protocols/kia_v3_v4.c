#include "kia_v3_v4.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Kia V3/V4: Later variants with different packet structures
// V3: 72-bit packets with additional fields
// V4: Similar but different button mapping

bool kia_v3_v4_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 24) return false;

    // Try different TEs
    uint32_t te_values[] = {200, 250, 180};
    
    for(int t = 0; t < 3; t++) {
        uint32_t te = te_values[t];
        uint64_t frame = 0;
        uint32_t bit_count = 0;
        uint32_t te_min = te * 60 / 100;
        uint32_t te_max = te * 160 / 100;
        uint32_t te2_min = te * 2 * 60 / 100;
        uint32_t te2_max = te * 2 * 160 / 100;

        // Find sync
        uint32_t start = 0;
        for(uint32_t i = 0; i < MIN(num_pulses, 8); i += 2) {
            if(i + 1 < num_pulses && pulses[i] > te * 3 && pulses[i + 1] > te * 3) {
                start = i + 2;
                break;
            }
        }

        // Decode Manchester
        for(uint32_t i = start; i < num_pulses - 1 && bit_count < 72; i += 2) {
            uint32_t p1 = pulses[i];
            uint32_t p2 = pulses[i + 1];
            if(p1 >= te_min && p1 <= te_max && p2 >= te2_min && p2 <= te2_max) {
                frame = (frame << 1) | 0;
                bit_count++;
            } else if(p1 >= te2_min && p1 <= te2_max && p2 >= te_min && p2 <= te_max) {
                frame = (frame << 1) | 1;
                bit_count++;
            } else {
                break;
            }
        }

        if(bit_count >= 64) {
            uint32_t serial = (uint32_t)((frame >> 32) & 0xFFFFFFFF);
            uint8_t button = (uint8_t)((frame >> 24) & 0x0F);
            uint16_t counter = (uint16_t)(frame & 0xFFFF);
            uint32_t encrypted = (uint32_t)(frame & 0xFFFFFFFF);

            result->serial = serial;
            result->button = button;
            result->counter = counter;
            result->crc = encrypted;
            result->valid = true;

            static ProtocolVersion pv;
            pv.manufacturer = ManufacturerKia;
            pv.variant = 3;
            pv.name = "KiaV3V4";
            pv.display_name = "Kia V3/V4";
            pv.features = PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ;
            pv.packet_len_bits = (uint8_t)bit_count;
            result->protocol = &pv;

            return true;
        }
    }

    return false;
}
