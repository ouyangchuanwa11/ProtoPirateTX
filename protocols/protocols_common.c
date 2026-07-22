#include "protocols_common.h"
#include "../defines.h"
#include "../helpers/protopirate_types.h"

#include <furi.h>
#include <string.h>

// Find pulse statistics
void protopirate_find_pulse_stats(const uint32_t* pulses, uint32_t num_pulses,
                                   uint32_t* min, uint32_t* max, uint32_t* avg) {
    if(!pulses || num_pulses == 0) {
        if(min) *min = 0;
        if(max) *max = 0;
        if(avg) *avg = 0;
        return;
    }

    uint32_t min_val = 0xFFFFFFFF;
    uint32_t max_val = 0;
    uint64_t sum = 0;

    for(uint32_t i = 0; i < num_pulses; i++) {
        uint32_t p = pulses[i];
        if(p < 50) continue; // Skip noise
        if(p < min_val) min_val = p;
        if(p > max_val) max_val = p;
        sum += p;
    }

    if(min) *min = (min_val == 0xFFFFFFFF) ? 0 : min_val;
    if(max) *max = max_val;
    if(avg) *avg = (uint32_t)(sum / num_pulses);
}

// Analyze pulse train to find timing parameters
bool protopirate_analyze_pulses(const uint32_t* pulses, uint32_t num_pulses,
                                 uint32_t* te, uint32_t* short_pulse, uint32_t* long_pulse) {
    if(!pulses || num_pulses < 4) return false;

    uint32_t min, max, avg;
    protopirate_find_pulse_stats(pulses, num_pulses, &min, &max, &avg);

    if(min == 0) return false;

    // The shortest pulse is usually TE
    uint32_t est_te = min;

    // Find the dominant short pulse (usually TE or TE*2)
    uint32_t histogram[20] = {0};
    for(uint32_t i = 0; i < MIN(num_pulses, 200); i++) {
        uint32_t ratio = pulses[i] / est_te;
        if(ratio < 20) histogram[ratio]++;
    }

    // Find most common ratios
    uint32_t best_ratio = 1;
    uint32_t best_count = 0;
    for(uint32_t r = 1; r < 10; r++) {
        if(histogram[r] > best_count) {
            best_count = histogram[r];
            best_ratio = r;
        }
    }

    if(te) *te = est_te;
    if(short_pulse) *short_pulse = est_te * best_ratio;
    if(long_pulse) *long_pulse = est_te * best_ratio * 3;

    return true;
}

// Decode KeeLoq 66-bit frame from Manchester encoded pulses
bool protopirate_decode_keeloq_frame(const uint32_t* pulses, uint32_t num_pulses,
                                      uint32_t te, uint64_t* frame_out) {
    if(!pulses || !frame_out || num_pulses < 10) return false;

    uint64_t frame = 0;
    uint32_t bit_count = 0;
    uint32_t te_min = te * 60 / 100;
    uint32_t te_max = te * 160 / 100;
    uint32_t te2_min = te * 2 * 60 / 100;
    uint32_t te2_max = te * 2 * 160 / 100;

    // Skip sync pulse (long pulse pair)
    uint32_t start_idx = 0;
    for(uint32_t i = 0; i < MIN(num_pulses, 10); i += 2) {
        if(i + 1 < num_pulses) {
            uint32_t p1 = pulses[i];
            uint32_t p2 = pulses[i + 1];
            // Look for sync: both long (4TE)
            if(p1 > te * 3 && p2 > te * 3) {
                start_idx = i + 2;
                break;
            }
        }
    }

    // Decode Manchester bits
    for(uint32_t i = start_idx; i < num_pulses - 1 && bit_count < 66; i += 2) {
        uint32_t p1 = pulses[i];
        uint32_t p2 = pulses[i + 1];

        if((p1 >= te_min && p1 <= te_max) && (p2 >= te2_min && p2 <= te2_max)) {
            // Short + Long = Bit 0
            frame = (frame << 1) | 0;
            bit_count++;
        } else if((p1 >= te2_min && p1 <= te2_max) && (p2 >= te_min && p2 <= te_max)) {
            // Long + Short = Bit 1
            frame = (frame << 1) | 1;
            bit_count++;
        } else {
            // Gap or noise - end of frame
            break;
        }
    }

    if(frame_out) *frame_out = frame;
    return bit_count >= 60; // Need at least 60 bits for valid KeeLoq
}

// Parse standard KeeLoq 66-bit frame
void protopirate_parse_keeloq_frame(uint64_t frame, uint32_t* serial,
                                     uint8_t* button, uint16_t* counter,
                                     uint32_t* encrypted) {
    // Standard KeeLoq 66-bit layout:
    // [63:62] = flags/button high
    // [61:60] = button
    // [59:48] = serial low 12 bits
    // [47:32] = serial high 16 bits
    // [31:0] = encrypted portion

    uint32_t enc = (uint32_t)(frame & 0xFFFFFFFF);
    uint32_t ser_high = (uint32_t)((frame >> 32) & 0xFFFF);
    uint32_t ser_low = (uint32_t)((frame >> 48) & 0xFFF);
    uint8_t btn = (uint8_t)((frame >> 60) & 0x03);

    if(serial) *serial = (ser_high << 12) | ser_low;
    if(button) *button = btn;
    if(counter) *counter = (uint16_t)((encrypted >> 16) & 0xFFFF);
    if(encrypted) *encrypted = enc;
}

// Get KeeLoq manufacturer key
bool protopirate_get_keeloq_key(uint32_t serial, Manufacturer manufacturer,
                                 uint32_t* key_l, uint32_t* key_h) {
    UNUSED(serial);

    // Manufacturer-specific KeeLoq keys
    // These are example/known keys - actual keys vary by manufacturer/region/year
    switch(manufacturer) {
    case ManufacturerKia:
        if(key_l) *key_l = 0xEADB4A;
        if(key_h) *key_h = 0x74967C;
        return true;
    case ManufacturerFord:
        if(key_l) *key_l = 0x123456;
        if(key_h) *key_h = 0xABCDEF;
        return true;
    case ManufacturerHonda:
        if(key_l) *key_l = 0xCAFEBABE;
        if(key_h) *key_h = 0xDEADBEEF;
        return true;
    case ManufacturerSubaru:
        if(key_l) *key_l = 0x87654321;
        if(key_h) *key_h = 0xFEDCBA98;
        return true;
    default:
        // Generic fallback
        if(key_l) *key_l = 0xEADB4A;
        if(key_h) *key_h = 0x74967C;
        return false;
    }
}

// Verify KeeLoq encrypted portion
bool protopirate_verify_keeloq(uint32_t encrypted, uint32_t serial,
                                uint8_t button, uint16_t counter,
                                uint32_t key_l, uint32_t key_h) {
    // Decrypt the encrypted portion
    uint32_t decrypted = protopirate_keeloq_decrypt(encrypted, key_l, key_h);

    // Extract fields from plaintext
    uint16_t dec_counter = (decrypted >> 16) & 0xFFFF;
    uint8_t dec_button = (decrypted >> 12) & 0x0F;
    uint16_t dec_serial = decrypted & 0xFFF;

    // Check if counter matches (within tolerance) and button matches
    // Note: serial_low might not match all the time depending on encoding
    if(abs((int32_t)dec_counter - (int32_t)counter) <= 500 &&
       dec_button == button) {
        return true;
    }

    // Some manufacturers encode differently, also check serial low bits
    if((dec_serial & 0xFFF) == (serial & 0xFFF) &&
       (dec_counter == (counter & 0xFFFF))) {
        return true;
    }

    return false;
}

// Decode PWM with sync detection
bool protopirate_decode_pwm_with_sync(const uint32_t* pulses, uint32_t num_pulses,
                                       uint32_t* data, uint32_t* bit_count,
                                       uint32_t te_short, uint32_t te_long) {
    if(!pulses || !data || !bit_count) return false;

    // Find sync pulse (a significantly longer pulse at the start)
    uint32_t start_idx = 0;
    uint32_t sync_threshold = te_long * 3;

    for(uint32_t i = 0; i < MIN(num_pulses, 10); i++) {
        if(pulses[i] > sync_threshold && pulses[i] < 100000) {
            start_idx = i + 1;
            break;
        }
    }

    uint32_t result = 0;
    uint32_t bits = 0;
    uint32_t short_min = te_short * 50 / 100;
    uint32_t short_max = te_short * 180 / 100;
    uint32_t long_min = te_long * 50 / 100;
    uint32_t long_max = te_long * 180 / 100;
    uint32_t gap_threshold = te_long * 3;

    for(uint32_t i = start_idx; i < num_pulses; i++) {
        uint32_t p = pulses[i];

        if(p >= short_min && p <= short_max) {
            result = (result << 1) | 0;
            bits++;
        } else if(p >= long_min && p <= long_max) {
            result = (result << 1) | 1;
            bits++;
        } else if(p > gap_threshold) {
            // Gap - end of packet
            break;
        } else {
            // Middle values - ambiguous, break
            break;
        }
    }

    *data = result;
    *bit_count = bits;
    return bits > 4;
}

// Decode Manchester with sync
bool protopirate_decode_manchester_with_sync(const uint32_t* pulses, uint32_t num_pulses,
                                              uint32_t* data, uint32_t* bit_count,
                                              uint32_t te) {
    if(!pulses || !data || !bit_count || num_pulses < 4) return false;

    // Find sync: look for long pulse pair (typically 4TE each)
    uint32_t start_idx = 0;
    uint32_t te_4 = te * 4;
    uint32_t te_4_min = te_4 * 60 / 100;
    uint32_t te_4_max = te_4 * 160 / 100;

    for(uint32_t i = 0; i < MIN(num_pulses, 10); i += 2) {
        if(i + 1 < num_pulses) {
            if(pulses[i] >= te_4_min && pulses[i] <= te_4_max &&
               pulses[i + 1] >= te_4_min && pulses[i + 1] <= te_4_max) {
                start_idx = i + 2;
                break;
            }
        }
    }

    // Now decode Manchester
    return protopirate_decode_manchester(pulses + start_idx, num_pulses - start_idx,
                                          te, data, bit_count);
}
