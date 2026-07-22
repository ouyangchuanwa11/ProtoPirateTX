#include "protopirate_types.h"
#include "../defines.h"

#include <furi.h>
#include <string.h>

// Reverse bits in a value
uint32_t protopirate_reverse_bits(uint32_t val, uint8_t bits) {
    uint32_t result = 0;
    for(uint8_t i = 0; i < bits; i++) {
        result = (result << 1) | (val & 1);
        val >>= 1;
    }
    return result;
}

// Extract bits from byte array
uint32_t protopirate_bytes_to_uint32(const uint8_t* data, uint8_t offset, uint8_t bits) {
    uint32_t result = 0;
    for(uint8_t i = 0; i < bits; i++) {
        uint8_t byte_idx = (offset + i) / 8;
        uint8_t bit_idx = (offset + i) % 8;
        result = (result << 1) | ((data[byte_idx] >> (7 - bit_idx)) & 1);
    }
    return result;
}

// Write bits to byte array
void protopirate_uint32_to_bytes(uint32_t val, uint8_t* data, uint8_t offset, uint8_t bits) {
    for(uint8_t i = 0; i < bits; i++) {
        uint8_t byte_idx = (offset + i) / 8;
        uint8_t bit_idx = (offset + i) % 8;
        uint32_t bit_val = (val >> (bits - 1 - i)) & 1;
        if(bit_val) {
            data[byte_idx] |= (1 << (7 - bit_idx));
        } else {
            data[byte_idx] &= ~(1 << (7 - bit_idx));
        }
    }
}

// Decode Manchester encoded data from pulse train
bool protopirate_decode_manchester(const uint32_t* pulses, uint32_t num_pulses,
                                    uint32_t te, uint32_t* out_bits, uint32_t* out_bit_count) {
    if(!pulses || num_pulses < 2) return false;

    uint32_t bits = 0;
    uint32_t bit_count = 0;

    for(uint32_t i = 0; i < num_pulses - 1; i += 2) {
        if(i + 1 >= num_pulses) break;

        // Manchester: short+short = 0 (high-low), long+long = 1 (low-high), or short+long
        // Actually for KeeLoq: short pulse (200us) followed by long (400us) = bit 0
        // long (400us) followed by short (200us) = bit 1
        uint32_t p1 = pulses[i];
        uint32_t p2 = pulses[i + 1];

        // Check tolerance (within 50%)
        uint32_t te_min = te * 70 / 100;
        uint32_t te_max = te * 150 / 100;
        uint32_t te2_min = te * 2 * 70 / 100;
        uint32_t te2_max = te * 2 * 150 / 100;

        if((p1 >= te_min && p1 <= te_max) && (p2 >= te2_min && p2 <= te2_max)) {
            // Short + Long = Bit 0
            bits = (bits << 1) | 0;
            bit_count++;
        } else if((p1 >= te2_min && p1 <= te2_max) && (p2 >= te_min && p2 <= te_max)) {
            // Long + Short = Bit 1
            bits = (bits << 1) | 1;
            bit_count++;
        } else if((p1 >= te_min && p1 <= te_max) && (p2 >= te_min && p2 <= te_max)) {
            // Both short - check parity or skip
            // Some protocols use this for sync
            continue;
        } else {
            // Unknown pattern
            break;
        }
    }

    if(out_bits) *out_bits = bits;
    if(out_bit_count) *out_bit_count = bit_count;
    return bit_count > 0;
}

// Encode data as Manchester pulses
uint32_t protopirate_encode_manchester(uint32_t data, uint32_t num_bits,
                                        uint32_t* pulses, uint32_t max_pulses, uint32_t te) {
    uint32_t count = 0;

    for(int32_t i = num_bits - 1; i >= 0 && count < max_pulses - 1; i--) {
        uint32_t bit = (data >> i) & 1;

        if(bit == 0) {
            // Bit 0: short high, long low
            if(count < max_pulses) pulses[count++] = te;
            if(count < max_pulses) pulses[count++] = te * 2;
        } else {
            // Bit 1: long high, short low
            if(count < max_pulses) pulses[count++] = te * 2;
            if(count < max_pulses) pulses[count++] = te;
        }
    }

    return count;
}

// Decode PWM data from pulse train
bool protopirate_decode_pwm(const uint32_t* pulses, uint32_t num_pulses,
                             uint32_t te_short, uint32_t te_long,
                             uint32_t* out_bits, uint32_t* out_bit_count) {
    if(!pulses || num_pulses == 0) return false;

    uint32_t bits = 0;
    uint32_t bit_count = 0;

    uint32_t short_min = te_short * 60 / 100;
    uint32_t short_max = te_short * 160 / 100;
    uint32_t long_min = te_long * 60 / 100;
    uint32_t long_max = te_long * 160 / 100;

    for(uint32_t i = 0; i < num_pulses; i++) {
        uint32_t p = pulses[i];

        if(p >= short_min && p <= short_max) {
            bits = (bits << 1) | 0;
            bit_count++;
        } else if(p >= long_min && p <= long_max) {
            bits = (bits << 1) | 1;
            bit_count++;
        } else if(p > long_max + 500) {
            // Gap signal - end of packet
            break;
        }
        // Values between short and long are ambiguous, skip
    }

    if(out_bits) *out_bits = bits;
    if(out_bit_count) *out_bit_count = bit_count;
    return bit_count > 0;
}

// Encode data as PWM pulses
uint32_t protopirate_encode_pwm(uint32_t data, uint32_t num_bits,
                                 uint32_t* pulses, uint32_t max_pulses,
                                 uint32_t te_short, uint32_t te_long, uint32_t gap) {
    uint32_t count = 0;

    for(int32_t i = num_bits - 1; i >= 0 && count < max_pulses - 1; i--) {
        uint32_t bit = (data >> i) & 1;
        if(count < max_pulses) {
            pulses[count++] = bit ? te_long : te_short;
        }
    }

    // Add gap at end
    if(count < max_pulses) {
        pulses[count++] = gap;
    }

    return count;
}

// CRC16 calculation
uint16_t protopirate_crc16(const uint8_t* data, uint32_t len, uint16_t poly, uint16_t init) {
    uint16_t crc = init;
    for(uint32_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x8000)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// CRC8 calculation
uint8_t protopirate_crc8(const uint8_t* data, uint32_t len, uint8_t poly, uint8_t init) {
    uint8_t crc = init;
    for(uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x80)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// KeeLoq NLF function (non-linear feedback)
static uint32_t keeloq_nlf(uint32_t x) {
    // KeeLoq NLF lookup table
    static const uint8_t nlf_table[64] = {
        0x33, 0x6B, 0xFD, 0x3B, 0x98, 0x4E, 0x99, 0x1B,
        0x97, 0x22, 0x6A, 0x72, 0xCC, 0x8C, 0x7E, 0x7B,
        0x11, 0x2E, 0x7A, 0x1F, 0x44, 0x69, 0x9E, 0x20,
        0x55, 0xB8, 0x32, 0xDD, 0x50, 0x32, 0x6F, 0xC4,
        0x8E, 0x34, 0x4D, 0x4D, 0x3D, 0x9B, 0x8B, 0xB1,
        0xA3, 0x30, 0x8F, 0x27, 0x9D, 0x90, 0x59, 0x8B,
        0xC7, 0xFE, 0xB4, 0x16, 0xFB, 0x90, 0x81, 0x39,
        0x8F, 0x24, 0x48, 0xEE, 0xFB, 0x82, 0x16, 0x61,
    };

    uint8_t idx = (x >> 2) & 0x3F; // bits 31..26 -> 6-bit index
    uint8_t nl = nlf_table[idx];
    uint32_t result = 0;
    for(int i = 0; i < 8; i++) {
        result = (result << 1) | ((nl >> i) & 1);
    }
    return result;
}

// KeeLoq encrypt 32-bit data with 64-bit key
uint32_t protopirate_keeloq_encrypt(uint32_t data, uint32_t key_l, uint32_t key_h) {
    uint32_t x = data;
    uint64_t key = ((uint64_t)key_h << 32) | key_l;

    for(uint16_t r = 0; r < 528; r++) {
        uint32_t nlf_out = keeloq_nlf(x);
        uint32_t key_bit = (key >> (r & 63)) & 1;
        uint32_t feedback = nlf_out ^ key_bit ^ ((x >> 16) & 1) ^ ((x >> 31) & 1);
        x = (x >> 1) | (feedback << 31);
    }

    return x;
}

// KeeLoq decrypt 32-bit data with 64-bit key
uint32_t protopirate_keeloq_decrypt(uint32_t data, uint32_t key_l, uint32_t key_h) {
    uint32_t x = data;
    uint64_t key = ((uint64_t)key_h << 32) | key_l;

    for(int16_t r = 527; r >= 0; r--) {
        uint32_t nlf_out = keeloq_nlf(x);
        uint32_t key_bit = (key >> (r & 63)) & 1;
        uint32_t fb_bit = (x >> 30) & 1;
        uint32_t feedback = nlf_out ^ key_bit ^ fb_bit ^ ((x >> 15) & 1);
        x = (x << 1) | feedback;
    }

    return x;
}

// Protocol-specific CRC calculators
// KeeLoq CRC: uses standard KeeLoq encrypt on serial + counter
uint32_t protopirate_calc_keeloq_crc(uint32_t serial, uint32_t button, uint32_t counter) {
    // The "CRC" in KeeLoq is the encrypted portion
    // For TX, we generate a new encrypted value using KeeLoq
    // Key is derived from manufacturer key and serial
    uint32_t key_l = 0xEADB4A; // Example manufacturer key (lower)
    uint32_t key_h = 0x74967C; // Example manufacturer key (upper)

    uint32_t plaintext = (counter << 16) | (button << 12) | (serial & 0xFFF);
    return protopirate_keeloq_encrypt(plaintext, key_l, key_h);
}

// Ford CRC: simple XOR checksum
uint32_t protopirate_calc_ford_crc(uint32_t serial, uint32_t button, uint32_t counter) {
    uint8_t data[8];
    memset(data, 0, 8);
    data[0] = (serial >> 24) & 0xFF;
    data[1] = (serial >> 16) & 0xFF;
    data[2] = (serial >> 8) & 0xFF;
    data[3] = serial & 0xFF;
    data[4] = (button & 0xFF);
    data[5] = (counter & 0xFF);
    data[6] = (counter >> 8) & 0xFF;

    uint8_t crc = 0;
    for(uint32_t i = 0; i < 7; i++) {
        crc ^= data[i];
    }
    return crc;
}

// Honda CRC: XOR-based with specific polynomial
uint32_t protopirate_calc_honda_crc(uint32_t serial, uint32_t button, uint32_t counter) {
    uint16_t crc = 0;
    crc ^= (serial >> 16) & 0xFFFF;
    crc ^= serial & 0xFFFF;
    crc ^= (counter & 0xFFFF);
    crc ^= (button & 0xFF) << 8;
    return crc & 0xFFFF;
}

// Build KeeLoq waveform for transmission
uint32_t protopirate_build_keeloq_waveform(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses) {
    if(!result || !result->valid) return 0;

    uint32_t count = 0;

    // KeeLoq frame structure:
    // Sync (4TE low + 4TE high) + Data (Manchester encoded, 66 bits) + Guard

    // Sync pulse: 4 * TE low
    if(count < max_pulses) pulses[count++] = KEELOQ_TE * 4;
    if(count < max_pulses) pulses[count++] = KEELOQ_TE * 4;

    // Build 66-bit KeeLoq frame
    // Bits: [31:0] encrypted, [15:0] serial_high, [11:0] serial_low, [2:0] button, [1:0] ?
    uint32_t encrypted = protopirate_keeloq_encrypt(
        (result->counter << 16) | (result->button << 12) | (result->serial & 0xFFF),
        0xEADB4A, 0x74967C);

    uint64_t frame = 0;
    frame = ((uint64_t)encrypted << 34) |
            ((uint64_t)((result->serial >> 12) & 0xFFFF) << 18) |
            ((uint64_t)(result->button & 0x03) << 16) |
            ((uint64_t)0xFFFF);

    // Manchester encode the frame
    for(int32_t i = 65; i >= 0 && count < max_pulses - 1; i--) {
        uint32_t bit = (frame >> i) & 1;
        if(bit) {
            pulses[count++] = KEELOQ_TE * 2; // Long
            pulses[count++] = KEELOQ_TE;     // Short
        } else {
            pulses[count++] = KEELOQ_TE;     // Short
            pulses[count++] = KEELOQ_TE * 2; // Long
        }
    }

    // Guard time
    if(count < max_pulses) pulses[count++] = KEELOQ_GAP;

    return count;
}

// Build fixed code waveform
uint32_t protopirate_build_fixed_waveform(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses) {
    if(!result || !result->valid) return 0;

    uint32_t count = 0;

    // Fixed code: typically PWM encoded
    // Tri-state bits: 0 = short, 1 = long, F = floating (gap)
    // Simple PWM: just send the fixed_code

    uint32_t data = result->fixed_code;
    uint32_t num_bits = 24; // Typical fixed code is 24 bits

    // Sync
    if(count < max_pulses) pulses[count++] = PWM_TE_SHORT;
    if(count < max_pulses) pulses[count++] = PWM_TE_SHORT * 10;

    for(int32_t i = num_bits - 1; i >= 0 && count < max_pulses; i--) {
        uint32_t bit = (data >> i) & 1;
        pulses[count++] = bit ? PWM_TE_LONG : PWM_TE_SHORT;
    }

    return count;
}

// Build general Manchester waveform
uint32_t protopirate_build_manchester_waveform(uint32_t data, uint32_t num_bits,
                                                uint32_t* pulses, uint32_t max_pulses, uint32_t te) {
    return protopirate_encode_manchester(data, num_bits, pulses, max_pulses, te);
}

// Find short pulse in a train (for auto-detection)
uint32_t protopirate_find_short_pulse(const uint32_t* pulses, uint32_t num_pulses) {
    if(!pulses || num_pulses == 0) return 300;

    uint32_t min_val = 0xFFFFFFFF;
    for(uint32_t i = 0; i < MIN(num_pulses, 100); i++) {
        if(pulses[i] > 50 && pulses[i] < min_val) {
            min_val = pulses[i];
        }
    }
    return min_val == 0xFFFFFFFF ? 300 : min_val;
}

// Find long pulse in a train
uint32_t protopirate_find_long_pulse(const uint32_t* pulses, uint32_t num_pulses) {
    if(!pulses || num_pulses == 0) return 900;

    uint32_t short_pulse = protopirate_find_short_pulse(pulses, num_pulses);
    uint32_t threshold = short_pulse * 15 / 10;
    uint32_t max_val = 0;

    for(uint32_t i = 0; i < MIN(num_pulses, 100); i++) {
        if(pulses[i] > threshold && pulses[i] < 5000 && pulses[i] > max_val) {
            max_val = pulses[i];
        }
    }
    return max_val == 0 ? short_pulse * 3 : max_val;
}

// Calculate timing element (short pulse average)
uint32_t protopirate_calculate_te(const uint32_t* pulses, uint32_t num_pulses) {
    return protopirate_find_short_pulse(pulses, num_pulses);
}
