#include "protopirate_rb.h"
#include <math.h>

// ===================== CRC8 =====================
uint8_t kia_crc8(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x80) crc = (crc << 1) ^ 0x7F;
            else crc <<= 1;
        }
    }
    return crc;
}

// ===================== 按钮名称 =====================
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

// ===================== 统合解码入口 =====================
DecodeResult* decode_signal(ProtoPirateApp* app, FuriString* raw_data) {
    if(!app || !raw_data) return NULL;
    
    DecodeResult* result = NULL;
    
    result = decode_kia_v0(app, raw_data);
    if(result) return result;
    
    result = decode_ford(app, raw_data);
    if(result) return result;
    
    result = decode_subaru(app, raw_data);
    if(result) return result;
    
    result = decode_fiat(app, raw_data);
    if(result) return result;
    
    result = decode_chrysler(app, raw_data);
    if(result) return result;
    
    result = decode_honda(app, raw_data);
    if(result) return result;
    
    result = decode_toyota(app, raw_data);
    if(result) return result;
    
    result = decode_starline(app, raw_data);
    if(result) return result;
    
    return NULL;
}

// ===================== Kia V0 (HiTag2 56-bit) =====================
DecodeResult* decode_kia_v0(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 350 && ad < 650) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 650 && ad < 1500) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 48) return NULL;
    
    // Extract fields
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    uint32_t serial = (hi << 4) | ((lo >> 28) & 0x0F);
    uint8_t button = (lo >> 24) & 0x0F;
    uint16_t counter = (lo >> 8) & 0xFFFF;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Kia V0 (HiTag2)", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = serial;
    result->button = button;
    strncpy(result->btn_name, get_button_name("Kia", button), sizeof(result->btn_name));
    result->counter = counter;
    result->encrypted = false;
    return result;
}

// ===================== Kia V1 =====================
DecodeResult* decode_kia_v1(ProtoPirateApp* app, FuriString* raw_data) {
    return decode_kia_v0(app, raw_data);
}

// ===================== Kia V2 =====================
DecodeResult* decode_kia_v2(ProtoPirateApp* app, FuriString* raw_data) {
    return decode_kia_v0(app, raw_data);
}

// ===================== Ford 40-bit =====================
DecodeResult* decode_ford(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 300 && ad < 600) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 600 && ad < 1400) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 36) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Ford", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = (hi << 12) | ((lo >> 20) & 0xFFF);
    result->button = (lo >> 17) & 0x07;
    result->counter = (lo >> 6) & 0x7FF;
    strncpy(result->btn_name, get_button_name("Ford", result->button), sizeof(result->btn_name));
    result->encrypted = false;
    return result;
}

// ===================== Subaru 56-bit =====================
DecodeResult* decode_subaru(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 250 && ad < 500) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 500 && ad < 1200) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 48) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Subaru", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = (hi << 16) | ((lo >> 16) & 0xFFFF);
    result->button = (lo >> 12) & 0x0F;
    result->counter = (lo >> 4) & 0xFF;
    strncpy(result->btn_name, get_button_name("Subaru", result->button), sizeof(result->btn_name));
    result->encrypted = false;
    return result;
}

// ===================== Fiat 40-bit =====================
DecodeResult* decode_fiat(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 200 && ad < 450) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 450 && ad < 1100) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 36) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Fiat", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = (hi << 12) | ((lo >> 20) & 0xFFF);
    result->button = (lo >> 16) & 0x0F;
    result->counter = (lo >> 8) & 0xFF;
    strncpy(result->btn_name, get_button_name("Fiat", result->button), sizeof(result->btn_name));
    result->encrypted = true;
    return result;
}

// ===================== Chrysler 60-bit =====================
DecodeResult* decode_chrysler(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 400 && ad < 800) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 800 && ad < 1800) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 52) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Chrysler", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = (hi << 16) | ((lo >> 16) & 0xFFFF);
    result->button = (lo >> 12) & 0x0F;
    result->counter = (lo >> 4) & 0xFF;
    strncpy(result->btn_name, get_button_name("Chrysler", result->button), sizeof(result->btn_name));
    result->encrypted = true;
    return result;
}

// ===================== Honda 40-bit =====================
DecodeResult* decode_honda(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 300 && ad < 550) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 550 && ad < 1300) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 36) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Honda", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = ((hi & 0xFF) << 16) | ((lo >> 16) & 0xFFFF);
    result->button = (lo >> 12) & 0x0F;
    result->counter = lo & 0xFFFF;
    strncpy(result->btn_name, get_button_name("Honda", result->button), sizeof(result->btn_name));
    result->encrypted = false;
    return result;
}

// ===================== Toyota 56-bit =====================
DecodeResult* decode_toyota(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 4000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 350 && ad < 600) { data = (data << 1) | 0; bits++; }
                else if(!lv && ad > 600 && ad < 1400) { data = (data << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 48) return NULL;
    
    uint32_t hi = (uint32_t)(data >> 32);
    uint32_t lo = (uint32_t)(data & 0xFFFFFFFF);
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "Toyota", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = hi;
    result->data_lo = lo;
    result->serial = (hi << 4) | ((lo >> 28) & 0x0F);
    result->button = (lo >> 24) & 0x0F;
    result->counter = (lo >> 8) & 0xFFFF;
    strncpy(result->btn_name, get_button_name("Toyota", result->button), sizeof(result->btn_name));
    result->encrypted = false;
    return result;
}

// ===================== StarLine 64-bit =====================
DecodeResult* decode_starline(ProtoPirateApp* app, FuriString* raw_data) {
    UNUSED(app);
    if(!raw_data) return NULL;
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 10) return NULL;
    
    uint64_t data_hi = 0, data_lo = 0;
    uint8_t bits = 0;
    int32_t val = 0;
    bool neg = false;
    uint8_t ph = 0;
    
    for(const char* p = str; *p; p++) {
        if(*p == '-') { neg = true; continue; }
        if(*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if(val != 0) {
            int32_t dur = neg ? -val : val; neg = false; val = 0;
            if(abs(dur) < 80 || abs(dur) > 3000) continue;
            uint16_t ad = abs(dur); bool lv = dur > 0;
            
            if(ph < 2) { ph++; }
            else {
                if(!lv && ad > 400 && ad < 600) { data_lo = (data_lo << 1) | 0; bits++; }
                else if(!lv && ad > 800 && ad < 1200) { data_lo = (data_lo << 1) | 1; bits++; }
                ph = 0;
            }
        }
    }
    if(bits < 32) return NULL;
    
    DecodeResult* result = malloc(sizeof(DecodeResult));
    memset(result, 0, sizeof(DecodeResult));
    strncpy(result->proto, "StarLine", sizeof(result->proto));
    result->bits = bits;
    result->data_hi = data_hi;
    result->data_lo = data_lo;
    result->serial = data_lo & 0xFFFFFFF;
    result->button = (data_lo >> 28) & 0x0F;
    strncpy(result->btn_name, get_button_name("StarLine", result->button), sizeof(result->btn_name));
    result->counter = (data_lo >> 8) & 0xFFFFF;
    result->encrypted = true;
    return result;
}