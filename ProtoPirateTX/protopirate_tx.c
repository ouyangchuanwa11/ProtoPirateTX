#include "protopirate_rb.h"
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/protocols/raw.h>
#include <furi.h>

#undef TAG
#define TAG "ProtoPirateTX"

// ===================== ⚠️ TX发射已启用 ⚠️ =====================
// g_device 在 app_main 时由 subghz_devices_get_by_name("cc1101_int") 初始化
// 不再为 NULL — 射频发送已放开

const SubGhzDevice* g_device = NULL;

typedef struct {
    uint32_t data[2];
    uint32_t repeat_pos;
    uint8_t repeats;
    uint8_t bit_pos;
    uint8_t phase;
    bool sending;
} TxState;

static TxState tx_state;
static FuriThread* g_tx_thread = NULL;

// OOK PWM timing parameters (microseconds)
#define BIT1_ON  560
#define BIT1_OFF 280
#define BIT0_ON  280
#define BIT0_OFF 560
#define PRE_ON   8000
#define PRE_OFF  4000
#define FRAME_GAP 12000

// ===================== 初始化设备接口 =====================
bool tx_device_init(void) {
    if(g_device) return true; // already initialized
    g_device = subghz_devices_get_by_name("cc1101_int");
    if(!g_device) {
        FURI_LOG_E(TAG, "❌ CC1101 NOT FOUND — is SubGHz module present?");
        return false;
    }
    FURI_LOG_I(TAG, "✅ CC1101 device acquired");
    return true;
}

void tx_device_deinit(void) {
    // Let each TX session manage begin/end; just clear ref here
    // The final end is called by whoever started the session
    g_device = NULL;
    FURI_LOG_I(TAG, "CC1101 device released");
}

// ===================== Level Duration callback for async TX =====================
static LevelDuration tx_callback(void* context) {
    UNUSED(context);
    if(!tx_state.sending) return level_duration_reset();
    
    if(tx_state.phase == 0) {
        tx_state.phase = 1;
        return level_duration_make(true, PRE_ON);
    }
    if(tx_state.phase == 1) {
        tx_state.phase = 2;
        tx_state.bit_pos = 0;
        return level_duration_make(false, PRE_OFF);
    }
    
    if(tx_state.bit_pos < 64) {
        uint32_t word = (tx_state.bit_pos < 32) ? tx_state.data[0] : tx_state.data[1];
        uint8_t bit = (word >> (31 - (tx_state.bit_pos % 32))) & 1;
        uint8_t is_on = (tx_state.bit_pos % 2 == 0);
        tx_state.bit_pos++;
        if(is_on) return level_duration_make(true, bit ? BIT1_ON : BIT0_ON);
        return level_duration_make(false, bit ? BIT1_OFF : BIT0_OFF);
    }
    
    tx_state.repeat_pos++;
    if(tx_state.repeat_pos >= tx_state.repeats) {
        tx_state.sending = false;
        return level_duration_reset();
    }
    tx_state.phase = 0;
    tx_state.bit_pos = 0;
    return level_duration_make(false, FRAME_GAP);
}

// ===================== TX Thread Worker =====================
static int32_t tx_thread_worker(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app || !g_device) return -1;
    
    FURI_LOG_I(TAG, "⚡ TX START: freq=%lu, data=0x%08lX%08lX, reps=%u",
               app->tx_freq, app->tx_data_hi, app->tx_data_lo, app->tx_repeats);
    
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, app->tx_freq);
    subghz_devices_set_tx(g_device);
    furi_delay_ms(50);
    
    tx_state.data[0] = app->tx_data_hi;
    tx_state.data[1] = app->tx_data_lo;
    tx_state.repeat_pos = 0;
    tx_state.repeats = app->tx_repeats;
    tx_state.phase = 0;
    tx_state.bit_pos = 0;
    tx_state.sending = true;
    
    subghz_devices_start_async_tx(g_device, tx_callback, NULL);
    
    // Wait with timeout
    uint32_t timeout = 5000; // 5 second max
    while(!subghz_devices_is_async_complete_tx(g_device) && timeout > 0) {
        furi_delay_ms(10);
        timeout -= 10;
    }
    
    if(!subghz_devices_is_async_complete_tx(g_device)) {
        subghz_devices_stop_async_tx(g_device);
        FURI_LOG_W(TAG, "⚠️ TX timeout — forced stop");
    }
    
    subghz_devices_idle(g_device);
    subghz_devices_end(g_device);
    
    app->tx_busy = false;
    FURI_LOG_I(TAG, "✅ TX DONE");
    return 0;
}

// ===================== TX Init HW (no longer disabled) =====================
bool tx_init_hw(ProtoPirateApp* app, uint32_t freq) {
    if(!g_device) {
        FURI_LOG_E(TAG, "❌ tx_init_hw: CC1101 not initialized");
        return false;
    }
    
    FURI_LOG_I(TAG, "📡 TX init: freq=%lu Hz", freq);
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, freq);
    furi_delay_ms(10);
    
    UNUSED(app);
    return true;
}

// ===================== Non-blocking transmit =====================
void transmit_packet_nonblock(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep) {
    if(!app) return;
    if(app->tx_busy) {
        FURI_LOG_W(TAG, "⚠️ TX busy, skipping");
        return;
    }
    
    app->tx_data_hi = dhi;
    app->tx_data_lo = dlo;
    app->tx_freq = freq;
    app->tx_repeats = rep;
    app->tx_busy = true;
    
    if(g_tx_thread) {
        furi_thread_join(g_tx_thread);
        furi_thread_free(g_tx_thread);
        g_tx_thread = NULL;
    }
    
    g_tx_thread = furi_thread_alloc_ex("ProtoPirateTX", 2048, tx_thread_worker, app);
    furi_thread_start(g_tx_thread);
}

// ===================== Wait for TX to complete =====================
void transmit_packet_wait(ProtoPirateApp* app) {
    UNUSED(app);
    if(g_tx_thread) {
        furi_thread_join(g_tx_thread);
        furi_thread_free(g_tx_thread);
        g_tx_thread = NULL;
    }
}

// ===================== Stop TX =====================
void transmit_packet_stop(ProtoPirateApp* app) {
    UNUSED(app);
    tx_state.sending = false;
    if(g_device && !subghz_devices_is_async_complete_tx(g_device)) {
        subghz_devices_stop_async_tx(g_device);
    }
    if(g_device) {
        subghz_devices_idle(g_device);
    }
    if(g_tx_thread) {
        furi_thread_join(g_tx_thread);
        furi_thread_free(g_tx_thread);
        g_tx_thread = NULL;
    }
    app->tx_busy = false;
}

// ===================== Blocking transmit =====================
bool transmit_packet(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep) {
    if(!app) return false;
    transmit_packet_nonblock(app, dhi, dlo, freq, rep);
    transmit_packet_wait(app);
    return true;
}

// ===================== Transmit Start (continuous session) =====================
void transmit_start(ProtoPirateApp* app, uint32_t freq) {
    if(!g_device || !app) return;
    FURI_LOG_I(TAG, "📡 TX continuous start: %lu Hz", freq);
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, freq);
    furi_delay_ms(10);
}

// ===================== Transmit Burst (within continuous session) =====================
void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo) {
    if(!g_device || !app) return;
    
    tx_state.data[0] = data_hi;
    tx_state.data[1] = data_lo;
    tx_state.repeat_pos = 0;
    tx_state.repeats = 3;
    tx_state.phase = 0;
    tx_state.bit_pos = 0;
    tx_state.sending = true;
    
    subghz_devices_set_tx(g_device);
    subghz_devices_start_async_tx(g_device, tx_callback, NULL);
    
    uint32_t timeout = 5000;
    while(!subghz_devices_is_async_complete_tx(g_device) && timeout > 0) {
        furi_delay_ms(10);
        timeout -= 10;
    }
    
    subghz_devices_stop_async_tx(g_device);
}

// ===================== Transmit Stop (end continuous session) =====================
void transmit_stop(ProtoPirateApp* app) {
    UNUSED(app);
    tx_state.sending = false;
    if(g_device) {
        subghz_devices_idle(g_device);
        subghz_devices_end(g_device);
    }
    FURI_LOG_I(TAG, "📡 TX continuous stop");
}

// ===================== Transmit RAW data =====================
bool transmit_raw(ProtoPirateApp* app, FuriString* raw_data, uint32_t freq, uint8_t repeats) {
    if(!app || !raw_data || !g_device) return false;
    
    const char* str = furi_string_get_cstr(raw_data);
    if(!str || strlen(str) < 5) return false;
    
    FURI_LOG_I(TAG, "📡 RAW TX: freq=%lu, len=%u, reps=%u", freq, (unsigned)strlen(str), repeats);
    
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, freq);
    
    for(uint8_t r = 0; r < repeats; r++) {
        subghz_devices_set_tx(g_device);
        
        // Manual GPIO bit-bang for raw pulse sequence
        int32_t val = 0;
        bool neg = false;
        bool level = false;
        furi_hal_gpio_write(&gpio_cc1101_g0, false);
        furi_delay_us(100);
        
        for(const char* p = str; *p; p++) {
            if(*p == '-') { neg = true; continue; }
            if(*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
            } else if(val != 0) {
                int32_t dur = neg ? -val : val;
                neg = false;
                val = 0;
                
                level = (dur > 0);
                uint32_t us = (uint32_t)(level ? dur : -dur);
                
                furi_hal_gpio_write(&gpio_cc1101_g0, level);
                furi_delay_us(us);
            }
        }
        if(val != 0) {
            int32_t dur = neg ? -val : val;
            level = (dur > 0);
            uint32_t us = (uint32_t)(level ? dur : -dur);
            furi_hal_gpio_write(&gpio_cc1101_g0, level);
            furi_delay_us(us);
        }
        
        furi_hal_gpio_write(&gpio_cc1101_g0, false);
        furi_delay_us(10000); // 10ms gap between repeats
        
        subghz_devices_idle(g_device);
    }
    
    subghz_devices_end(g_device);
    FURI_LOG_I(TAG, "✅ RAW TX DONE: %u repeats", repeats);
    return true;
}
