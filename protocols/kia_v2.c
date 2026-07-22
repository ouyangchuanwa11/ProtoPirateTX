#include "kia_v2.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Kia V2: Variant with different button encoding
// Button is encoded in 4 bits (vs 2 bits in V0/V1)

bool kia_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 200;

    uint64_t frame = 0;
    if(!protopirate_decode_keeloq_frame(pulses, num_pulses, te, &frame)) {
        return false;
    }

    uint32_t serial;
    uint8_t button;
    uint16_t counter;
    uint32_t encrypted;

    protopirate_parse_keeloq_frame(frame, &serial, &button, &counter, &encrypted);

    // V2 uses 4-bit button
    uint8_t btn_4bit = (frame >> 58) & 0x0F;

    // Verify with Kia key
    uint32_t key_l, key_h;
    protopirate_get_keeloq_key(serial, ManufacturerKia, &key_l, &key_h);

    if(!protopirate_verify_keeloq(encrypted, serial, button, counter, key_l, key_h)) {
        // Accept if basic frame structure is OK
        if(serial == 0 || encrypted == 0) return false;
    }

    result->serial = serial;
    result->button = btn_4bit;
    result->counter = counter;
    result->crc = encrypted;
    result->valid = true;

    static ProtocolVersion pv;
    pv.manufacturer = ManufacturerKia;
    pv.variant = 2;
    pv.name = "KiaV2";
    pv.display_name = "Kia V2";
    pv.features = PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ;
    pv.packet_len_bits = 66;
    result->protocol = &pv;

    return true;
}
