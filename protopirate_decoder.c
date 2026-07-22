#include "protopirate_rb.h"

// ===================== CRC8 =====================
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

// ===================== 按键名称 =====================
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
    }
    return "Btn:??";
}

// ===================== Kia V0 解码 =====================
DecodeResult* decode_kia_v0(ProtoPirateApp* app, FuriString* raw_str) {
    UNUSED(app);
    const char* str = furi_string_get_cstr(raw_str);
    uint32_t data_hi = 0, data_lo = 0;
    uint16_t bit_count = 0;
    int16_t prev_dur = 0;
    uint16_t header_count = 0;
    uint8_t step = 0;

    const uint16_t te_short = 250, te_long = 500, te_delta = 100;
    const uint8_t min_bits = 61;

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
                    step = 1;
                    prev_dur = abs_dur;
                    header_count = 0;
                }
            } else if(step == 1) {
                if(level) {
                    prev_dur = abs_dur;
                } else {
                    if(abs((int16_t)(abs_dur - te_short)) < te_delta &&
                       abs((int16_t)(prev_dur - te_short)) < te_delta) {
                        header_count++;
                    } else if(abs((int16_t)(abs_dur - te_long)) < te_delta &&
                              abs((int16_t)(prev_dur - te_long)) < te_delta) {
                        if(header_count > 15) {
                            step = 2;
                            data_hi = 0;
                            data_lo = 1;
                            bit_count = 1;
                        } else {
                            step = 0;
                        }
                    } else {
                        step = 0;
                    }
                }
            } else if(step == 2) {
                if(level) {
                    if(abs_dur >= te_long + te_delta * 2) {
                        if(bit_count == min_bits) goto decode_done;
                        step = 0;
                    } else {
                        prev_dur = abs_dur;
                        step = 3;
                    }
                } else {
                    step = 0;
                }
            } else if(step == 3) {
                if(!level) {
                    if(abs((int16_t)(prev_dur - te_short)) < te_delta &&
                       abs((int16_t)(abs_dur - te_short)) < te_delta) {
                        data_lo = (data_lo << 1) | 0;
                        bit_count++;
                        step = 2;
                    } else if(abs((int16_t)(prev_dur - te_long)) < te_delta &&
                              abs((int16_t)(abs_dur - te_long)) < te_delta) {
                        data_lo = (data_lo << 1) | 1;
                        bit_count++;
                        step = 2;
                    } else {
                        step = 0;
                    }
                } else {
                    step = 0;
                }
            }
            if(bit_count > 66) break;
        }
    }

decode_done:
    if(bit_count < min_bits) return NULL;

    uint32_t serial = (data_lo >> 12) & 0x0FFFFFFF;
    uint8_t button = (data_lo >> 8) & 0x0F;
    uint16_t counter = (data_hi << 24 | (data_lo >> 8)) >> 16 & 0xFFFF;
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
    result->encrypted = true;
    return result;
}

// ---------- 占位解码器 ----------
DecodeResult* decode_kia_v1(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_kia_v2(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_ford(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_starline(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_subaru(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_fiat(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }
DecodeResult* decode_chrysler(ProtoPirateApp* app, FuriString* raw_str) { UNUSED(app); UNUSED(raw_str); return NULL; }

// ===================== 主解码入口 =====================
DecodeResult* decode_signal(ProtoPirateApp* app, FuriString* raw_data) {
    DecodeResult* result = NULL;
    result = decode_kia_v0(app, raw_data); if(result) return result;
    result = decode_kia_v1(app, raw_data); if(result) return result;
    result = decode_kia_v2(app, raw_data); if(result) return result;
    result = decode_ford(app, raw_data); if(result) return result;
    result = decode_starline(app, raw_data); if(result) return result;
    result = decode_subaru(app, raw_data); if(result) return result;
    result = decode_fiat(app, raw_data); if(result) return result;
    result = decode_chrysler(app, raw_data); if(result) return result;
    return NULL;
}
