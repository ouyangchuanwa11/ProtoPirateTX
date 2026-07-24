#include "protopirate_rb.h"
#include <math.h>

// ===================== CRC8 (Kia  0x7F) =====================
uint8_t kia_crc8(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x7F;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// =====================  =====================
const char* get_button_name(const char* proto, uint8_t btn) {
    if(strstr(proto, "Kia")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 3) return "Trunk";
        if(btn == 4) return "Panic";
    } else if(strstr(proto, "Ford")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 4) return "Boot";
    } else if(strstr(proto, "Subaru")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 4) return "Trunk";
        if(btn == 8) return "Panic";
    } else if(strstr(proto, "Fiat")) {
        if(btn == 1) return "Unlock";
        if(btn == 2) return "Lock";
        if(btn == 4) return "Boot";
    } else if(strstr(proto, "Chrysler")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 4) return "Trunk";
        if(btn == 8) return "Panic";
    } else if(strstr(proto, "Honda")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 4) return "Trunk";
        if(btn == 8) return "Panic";
    } else if(strstr(proto, "Toyota")) {
        if(btn == 1) return "Lock";
        if(btn == 2) return "Unlock";
        if(btn == 4) return "Trunk";
    } else if(strstr(proto, "StarLine")) {
        if(btn == 1) return "Arm";
        if(btn == 2) return "Disarm";
        if(btn == 4) return "Trunk";
        if(btn == 8) return "Start";
    }
    return "Btn:??";
}

// =====================  OOK  =====================
// : "123 -456 789..." (=, =)
// : data_hi/data_lo +
DecodeResult* decode_raw_ook(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    if(!raw_str || furi_string_empty(raw_str)) return NULL;
    
    const char* str = furi_string_get_cstr(raw_str);
    FURI_LOG_I(TAG, "decode_raw_ook: %s", str);
    
    
    uint16_t count = 0;
    int32_t vals[512];
    int32_t val = 0;
    bool neg = false;
    
    for(const char* p = str; *p && count < 512; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            vals[count++] = neg ? -val : val;
            neg = false;
            val = 0;
        }
    }
    if(val != 0 && count < 512) vals[count++] = neg ? -val : val;
    
    if(count < 16) return NULL;
    
    
    uint32_t sum_pos = 0, sum_neg = 0;
    uint16_t n_pos = 0, n_neg = 0;
    uint32_t min_dur = 100000, max_dur = 0;
    
    for(uint16_t i = 0; i < count; i++) {
        uint32_t d = abs(vals[i]);
        if(d < min_dur) min_dur = d;
        if(d > max_dur) max_dur = d;
        if(vals[i] > 0) { sum_pos += d; n_pos++; }
        else { sum_neg += d; n_neg++; }
    }
    
    // OOK
    // : (~280-560us) + (~520-1200us)
    uint16_t short_count = 0, long_count = 0; (void)short_count; (void)long_count;
    uint32_t avg_short = 0, avg_long = 0;
    
    
    uint32_t total = 0;
    for(uint16_t i = 0; i < count; i++) {
        uint32_t d = abs(vals[i]);
        total += d;
    }
    uint32_t avg = count > 0 ? total / count : 0;
    
    uint32_t cluster1_sum = 0, cluster2_sum = 0;
    uint16_t cluster1_n = 0, cluster2_n = 0;
    
    for(uint16_t i = 0; i < count; i++) {
        uint32_t d = abs(vals[i]);
        if(d <= avg) { cluster1_sum += d; cluster1_n++; }
        else { cluster2_sum += d; cluster2_n++; }
    }
    
    if(cluster1_n > 0 && cluster2_n > 0) {
        avg_short = cluster1_sum / cluster1_n;
        avg_long = cluster2_sum / cluster2_n;
    } else {
        return NULL;
    }
    
    // OOK
    if(avg_long < avg_short * 1.5 || avg_short < 80) return NULL;
    
    // OOK
    
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bits = 0;
    uint16_t threshold = (avg_short + avg_long) / 2;
    
    // Manchester-like
    
    uint16_t bit_start = 0;
    for(uint16_t i = 0; i < count - 3; i++) {
        uint32_t d1 = abs(vals[i]), d2 = abs(vals[i+1]);
        uint32_t d3 = abs(vals[i+2]), d4 = abs(vals[i+3]);
        // =
        if(abs((int32_t)(d1 - d4)) < 100 && abs((int32_t)(d2 - d3)) < 100 &&
           d1 > threshold && d2 < threshold && d3 > threshold && d4 < threshold) {
            if(i > 20) { 
                bit_start = i + 4;
                break;
            }
        }
    }
    
    
    if(bit_start == 0) bit_start = 4;
    if(bit_start >= count) bit_start = count / 2;
    
    
    for(uint16_t i = bit_start; i < count - 1 && bits < 64; i += 2) {
        if(i + 1 >= count) break;
        uint32_t d1 = abs(vals[i]), d2 = abs(vals[i + 1]);
        bool level1 = vals[i] > 0, level2 = vals[i + 1] > 0;
        
        if(!level1 && level2) {
            
            if(d1 < threshold && d2 > threshold) {
                // + = 0
                if(d1 < avg_long && d2 < avg_long) {
                    data_lo = (data_lo << 1) | 0;
                    bits++;
                }
            } else if(d1 > threshold && d2 > threshold) {
                // + = 1
                if(d1 >= avg_short && d2 >= avg_short) {
                    // 1
                }
            }
        } else if(level1 && !level2) {
            
            if(d1 < threshold && d2 < threshold) {
                data_lo = (data_lo << 1) | 1;
                bits++;
            } else if(d1 >= avg_short && d2 >= avg_short) {
                data_lo = (data_lo << 1) | 0;
                bits++;
            }
        }
        if(bits > 66) break;
    }
    
    // OOK
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    
    if(bits >= 24) {
        strncpy(result->proto, "Raw OOK", sizeof(result->proto));
        result->bits = bits;
        result->data_hi = data_hi;
        result->data_lo = data_lo;
        result->crc_ok = false;
        result->encrypted = true;
        // serial  counter
        uint32_t raw_frame = data_lo;
        result->serial = raw_frame & 0xFFFFF;
        result->counter = (raw_frame >> 20) & 0xFFF;
    } else {
        // -  Unknown OOK
        strncpy(result->proto, "Unknown OOK", sizeof(result->proto));
        result->bits = count;
        result->data_hi = 0;
        result->data_lo = 0;
        result->serial = 0;
        result->counter = 0;
        result->crc_ok = false;
        result->encrypted = true;
    }
    
    return result;
}

// ===================== Kia V0 (HiTag2 / HCS)  =====================
DecodeResult* decode_kia_v0(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    if(!raw_str || furi_string_empty(raw_str)) return NULL;
    if(furi_string_equal_str(raw_str, "DEMO")) return NULL;

    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 10) return NULL;

    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    int16_t prev_dur = 0; (void)prev_dur;
    uint16_t header_count = 0;
    uint8_t step = 0;
    const uint16_t te_short = 260, te_long = 520, te_delta = 120;
    const uint8_t min_bits = 60;
    int32_t val = 0;
    bool negative = false;

    for(const char* p = str; *p; p++) {
        if(*p == '-') { negative = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0 || *p == ' ') {
            if(val == 0) { negative = false; continue; }
            int32_t dur = negative ? -val : val;
            negative = false;
            val = 0;
            if(abs(dur) < 50 || abs(dur) > 5000) continue;
            uint16_t abs_dur = abs(dur);
            bool level = dur > 0;

            if(step == 0) {
                if(level && abs((int16_t)(abs_dur - te_short)) < te_delta) {
                    step = 1; prev_dur = abs_dur; header_count = 0;
                }
            } else if(step == 1) {
                if(level) { prev_dur = abs_dur; }
                else {
                    if(abs((int16_t)(abs_dur - te_short)) < te_delta &&
                       abs((int16_t)(prev_dur - te_short)) < te_delta) {
                        header_count++;
                    } else if(abs((int16_t)(abs_dur - te_long)) < te_delta &&
                              abs((int16_t)(prev_dur - te_long)) < te_delta) {
                        if(header_count > 15) { step = 2; data_hi = 0; data_lo = 0; bit_count = 0; }
                        else { step = 0; }
                    } else { step = 0; }
                }
            } else if(step == 2) {
                if(level) { prev_dur = abs_dur; step = 3; } else { step = 0; }
            } else if(step == 3) {
                if(!level) {
                    if(abs((int16_t)(prev_dur - te_short)) < te_delta &&
                       abs((int16_t)(abs_dur - te_short)) < te_delta) {
                        data_lo = (data_lo << 1) | 0; bit_count++; step = 2;
                    }
                    else if(abs((int16_t)(prev_dur - te_long)) < te_delta &&
                            abs((int16_t)(abs_dur - te_long)) < te_delta) {
                        data_lo = (data_lo << 1) | 1; bit_count++; step = 2;
                    } else { step = 0; }
                } else { step = 0; }
            }
            if(bit_count > 66) break;
        }
    }

    if(bit_count < min_bits) return NULL;

    // HiTag2
    uint32_t serial = (data_lo >> 12) & 0x0FFFFFFF;
    uint8_t button = (data_lo >> 8) & 0x0F;
    uint16_t counter = ((data_hi << 24) | (data_lo >> 8)) >> 16 & 0xFFFF;
    uint8_t rxcrc = data_lo & 0xFF;

    uint8_t crc_bytes[6] = {0};
    crc_bytes[0] = (data_hi >> 16) & 0xFF;
    crc_bytes[1] = (data_hi >> 8) & 0xFF;
    crc_bytes[2] = data_hi & 0xFF;
    crc_bytes[3] = (data_lo >> 24) & 0xFF;
    crc_bytes[4] = (data_lo >> 16) & 0xFF;
    crc_bytes[5] = (data_lo >> 8) & 0xFF;
    uint8_t calc_crc = kia_crc8(crc_bytes, 6);

    DecodeResult* result = malloc(sizeof(DecodeResult));
    strncpy(result->proto, "Kia V0", sizeof(result->proto));
    result->bits = bit_count;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = serial;
    result->button = button;
    strncpy(result->btn_name, get_button_name("Kia V0", button), sizeof(result->btn_name));
    result->counter = counter;
    result->crc_ok = (rxcrc == calc_crc);
    result->encrypted = false;
    return result;
}

// ===================== Ford  =====================
DecodeResult* decode_ford(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 20) return NULL;
    
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    int16_t prev_dur = 0; (void)prev_dur;
    uint8_t step = 0;
    
    // Ford  PWM : bit1 = 600+200, bit0 = 200+600
    const uint16_t te_short = 200, te_long = 600, te_delta = 100;
    const uint8_t min_bits = 24;
    int32_t val = 0;
    bool neg = false;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); }
        else if(val != 0) {
            int32_t dur = neg ? -val : val;
            neg = false; val = 0;
            if(abs(dur) < 60 || abs(dur) > 3000) continue;
            uint16_t ad = abs(dur);
            bool lv = dur > 0;
            
            if(step == 0) {
                if(lv && abs((int16_t)(ad - te_long)) < te_delta) {
                    step = 1; prev_dur = ad;
                }
            } else if(step == 1) {
                if(!lv && abs((int16_t)(ad - te_short)) < te_delta) {
                    // bit 1
                    data_lo = (data_lo << 1) | 1; bit_count++;
                    step = 0;
                } else if(!lv && abs((int16_t)(ad - te_long)) < te_delta) {
                    step = 2; prev_dur = ad;
                } else { step = 0; }
            } else if(step == 2) {
                if(lv && abs((int16_t)(ad - te_short)) < te_delta) {
                    data_lo = (data_lo << 1) | 0; bit_count++;
                }
                step = 0;
            }
            if(bit_count > 66) break;
        }
    }
    
    if(bit_count < min_bits) return NULL;
    
    // Ford : [40-bit]  serial + counter + button
    uint32_t serial = data_lo & 0x1FFFFF;
    uint16_t counter = (data_lo >> 21) & 0x7FF;
    uint8_t button = (data_lo >> 6) & 0x0F;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    strncpy(result->proto, "Ford", sizeof(result->proto));
    result->bits = bit_count;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = serial;
    result->button = button;
    strncpy(result->btn_name, get_button_name("Ford", button), sizeof(result->btn_name));
    result->counter = counter;
    result->crc_ok = true;
    result->encrypted = true;
    return result;
}

// ===================== Subaru  =====================
DecodeResult* decode_subaru(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 20) return NULL;
    
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    bool found = false; (void)found;
    
    // Subaru: Manchester , 400/800us, 56
    const uint16_t te = 400, te_delta = 100;
    int32_t val = 0; bool neg = false; int16_t prev = 0; (void)prev; uint8_t ph = 0;
    uint16_t sync_cnt = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); }
        else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 2000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph == 0) {
                if(!lv && abs((int16_t)(ad - te)) < te_delta) {
                    ph = 1; prev = ad;
                }
            } else if(ph == 1) {
                if(lv && abs((int16_t)(ad - te)) < te_delta) {
                    sync_cnt++;
                    if(sync_cnt > 10) { ph = 2; sync_cnt = 0; }
                } else { ph = 0; sync_cnt = 0; }
                prev = ad;
            } else if(ph == 2) {
                if(!lv && abs((int16_t)(ad - te)) < te_delta) {
                    ph = 3; prev = ad;
                } else { ph = 0; }
            } else if(ph == 3) {
                if(lv) {
                    if(abs((int16_t)(ad - te)) < te_delta) {
                        if(abs((int16_t)(prev - te)) < te_delta) {
                            data_lo = (data_lo << 1) | 0; bit_count++; ph = 2;
                        } else if(abs((int16_t)(prev - 2 * te)) < te_delta) {
                            data_lo = (data_lo << 1) | 1; bit_count++; ph = 2;
                        } else { ph = 0; }
                    } else { ph = 0; }
                } else { ph = 0; }
            }
            if(bit_count > 66) break;
        }
    }
    
    if(bit_count < 30) return NULL;
    
    uint32_t serial = data_lo & 0xFFFFFF;
    uint8_t button = (data_lo >> 24) & 0x0F;
    uint16_t counter = (data_lo >> 8) & 0xFFFF;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    strncpy(result->proto, "Subaru", sizeof(result->proto));
    result->bits = bit_count;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = serial;
    result->button = button;
    strncpy(result->btn_name, get_button_name("Subaru", button), sizeof(result->btn_name));
    result->counter = counter;
    result->crc_ok = true;
    result->encrypted = true;
    return result;
}

// ===================== Fiat  =====================
DecodeResult* decode_fiat(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 20) return NULL;
    
    // Fiat:  Chrysler,  PWM
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    DecodeResult* result = malloc(sizeof(DecodeResult));
    
    // :  OOK
    int32_t val = 0; bool neg = false;
    uint8_t ph = 0;
    int16_t prev = 0; (void)prev;
    const uint16_t te = 300, delta = 100;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); }
        else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph == 0) {
                if(lv && ad > 5000) { ph = 1; }
            } else if(ph == 1) {
                if(!lv && abs((int16_t)(ad - te)) < delta) { ph = 2; prev = ad; }
            } else if(ph == 2) {
                if(lv) {
                    if(abs((int16_t)(ad - te)) < delta) { data_lo = (data_lo<<1)|1; bit_count++; ph = 1; }
                    else if(abs((int16_t)(ad - te*2)) < delta) { data_lo = (data_lo<<1)|0; bit_count++; ph = 1; }
                    else { ph = 0; }
                } else { ph = 0; }
            }
            if(bit_count > 66) break;
        }
    }
    
    if(bit_count < 20) { free(result); return NULL; }
    
    strncpy(result->proto, "Fiat", sizeof(result->proto));
    result->bits = bit_count;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = data_lo & 0xFFFFF;
    result->button = (data_lo >> 20) & 0x0F;
    strncpy(result->btn_name, get_button_name("Fiat", result->button), sizeof(result->btn_name));
    result->counter = (data_lo >> 24) & 0xFF;
    result->crc_ok = false;
    result->encrypted = true;
    return result;
}

// ===================== Chrysler  =====================
DecodeResult* decode_chrysler(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 20) return NULL;
    
    // Chrysler: PWM ,  60-66
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    int32_t val = 0; bool neg = false; uint8_t ph = 0; int16_t prev = 0; (void)prev;
    const uint16_t te = 300, delta = 80;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); }
        else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph == 0) {
                if(lv && ad > 2000) { ph = 1; }
            } else if(ph == 1) {
                if(!lv) { prev = ad; ph = 2; }
            } else if(ph == 2) {
                if(lv) {
                    uint16_t total = prev + ad;
                    if(abs((int16_t)(total - te*2)) < delta*2) {
                        // bit
                        if(abs((int16_t)(ad - te)) < delta) { data_lo = (data_lo<<1)|0; bit_count++; }
                        else if(abs((int16_t)(ad - te*2)) < delta) { data_lo = (data_lo<<1)|1; bit_count++; }
                        ph = 1;
                    } else { ph = 0; }
                } else { ph = 0; }
            }
            if(bit_count > 66) break;
        }
    }
    
    if(bit_count < 20) return NULL;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    strncpy(result->proto, "Chrysler", sizeof(result->proto));
    result->bits = bit_count;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = data_lo & 0xFFFFFF;
    result->button = (data_lo >> 24) & 0x0F;
    strncpy(result->btn_name, get_button_name("Chrysler", result->button), sizeof(result->btn_name));
    result->counter = (data_lo >> 16) & 0xFF;
    result->crc_ok = false;
    result->encrypted = true;
    return result;
}

// ===================== StarLine  =====================
DecodeResult* decode_starline(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    // StarLine: ,
    // -
    const char* str = furi_string_get_cstr(raw_str);
    if(!str || strlen(str) < 30) return NULL;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    strncpy(result->proto, "StarLine", sizeof(result->proto));
    result->bits = 0;
    result->data_hi = 0;
    result->data_lo = 0;
    result->serial = 0;
    result->button = 0;
    strncpy(result->btn_name, "Unknown", sizeof(result->btn_name));
    result->counter = 0;
    result->crc_ok = false;
    result->encrypted = true;
    return result;
}

// =====================  =====================

DecodeResult* decode_signal(ProtoPirateApp* app, FuriString* raw_data) {
    if(!raw_data || furi_string_empty(raw_data)) return NULL;
    
    FURI_LOG_I(TAG, "decode_signal: %s", furi_string_get_cstr(raw_data));
    
    DecodeResult* result = NULL;
    
    
    result = decode_kia_v0(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Kia V0"); return result; }
    
    result = decode_ford(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Ford"); return result; }
    
    result = decode_subaru(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Subaru"); return result; }
    
    result = decode_fiat(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Fiat"); return result; }
    
    result = decode_chrysler(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Chrysler"); return result; }
    
    result = decode_starline(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: StarLine"); return result; }
    
    // :  OOK
    result = decode_raw_ook(app, raw_data);
    if(result) { FURI_LOG_I(TAG, "Decoded: Raw OOK"); return result; }
    
    FURI_LOG_W(TAG, "decode_signal: no match");
    return NULL;
}
