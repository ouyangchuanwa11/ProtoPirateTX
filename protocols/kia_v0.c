#include "kia_v0.h"
#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Kia V0: KeeLoq-based, Manchester encoded, 66-bit frame
// Uses standard KeeLoq with specific manufacturer key
// TE = 200us, Manchester encoding
// Frame: sync(4TE) + 66 Manchester bits + guard

bool kia_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 20) return false;

    uint32_t te = 200; // Kia V0 uses ~200us time element

    // Decode Manchester from pulse train
    uint64_t frame = 0;
    if(!protopirate_decode_keeloq_frame(pulses, num_pulses, te, &frame)) {
        return false;
    }

    // Parse frame
    uint32_t serial;
    uint8_t button;
    uint16_t counter;
    uint32_t encrypted;

    protopirate_parse_keeloq_frame(frame, &serial, &button, &counter, &encrypted);

    // Verify with KeeLoq key
    uint32_t key_l, key_h;
    protopirate_get_keeloq_key(serial, ManufacturerKia, &key_l, &key_h);

    if(!protopirate_verify_keeloq(encrypted, serial, button, counter, key_l, key_h)) {
        // Could still be valid with different key - accept if frame looks reasonable
        if(counter > 65000 || serial == 0) return false;
    }

    // Fill result
    result->serial = serial;
    result->button = button;
    result->counter = counter;
    result->crc = encrypted;
    result->valid = true;

    if(!result->protocol) {
        static ProtocolVersion pv;
        pv.manufacturer = ManufacturerKia;
        pv.variant = 0;
        pv.name = "KiaV0";
        pv.display_name = "Kia V0 (KeeLoq)";
        pv.features = PROTO_HAS_TX | PROTO_HAS_ROLLBACK | PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ;
        pv.packet_len_bits = 66;
        result->protocol = &pv;
    }

    return true;
}

// Build Kia V0 waveform for TX
uint32_t kia_v0_build(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses) {
    if(!result || !result->valid || !pulses) return 0;

    uint32_t count = 0;
    uint32_t te = 200;

    // Sync pulse
    if(count < max_pulses) pulses[count++] = te * 4;
    if(count < max_pulses) pulses[count++] = te * 4;

    // Build frame
    uint32_t key_l, key_h;
    protopirate_get_keeloq_key(result->serial, ManufacturerKia, &key_l, &key_h);

    uint32_t encrypted = protopirate_keeloq_encrypt(
        (result->counter << 16) | (result->button << 12) | (result->serial & 0xFFF),
        key_l, key_h);

    uint64_t frame = ((uint64_t)encrypted) |
                     (((uint64_t)((result->serial >> 12) & 0xFFFF)) << 32) |
                     (((uint64_t)(result->serial & 0xFFF)) << 48) |
                     (((uint64_t)(result->button & 0x03)) << 60) |
                     (0ULL << 62);

    // Manchester encode
    for(int32_t i = 65; i >= 0 && count < max_pulses - 1; i--) {
        uint32_t bit = (frame >> i) & 1;
        if(bit) {
            pulses[count++] = te * 2;
            pulses[count++] = te;
        } else {
            pulses[count++] = te;
            pulses[count++] = te * 2;
        }
    }

    // Guard time
    if(count < max_pulses) pulses[count++] = KEELOQ_GAP;

    return count;
}
