#include "protopirate_rb.h"
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/protocols/raw.h>
#include <furi.h>

#undef TAG
#define TAG "ProtoPirateTX"

static const SubGhzDevice* g_device = NULL;

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

#define BIT1_ON  560
#define BIT1_OFF 280
#define BIT0_ON  280
#define BIT0_OFF 560
#define PRE_ON   8000
#define PRE_OFF  4000
#define FRAME_GAP 12000

static LevelDuration tx_callback(void* context) {
    UNUSED(context);
    if(!tx_state.sending) return level_duration_reset();
    if(tx_state.phase == 0) { tx_state.phase = 1; return level_duration_make(true, PRE_ON); }
    if(tx_state.phase == 1) { tx_state.phase = 2; tx_state.bit_pos = 0; return level_duration_make(false, PRE_OFF); }
    if(tx_state.bit_pos < 64) {
        uint32_t word = (tx_state.bit_pos < 32) ? tx_state.data[0] : tx_state.data[1];
        uint8_t bit = (word >> (31 - tx_state.bit_pos % 32)) & 1;
        uint8_t is_on = tx_state.bit_pos % 2 ? 0 : 1;
        tx_state.bit_pos++;
        if(is_on) return level_duration_make(true, bit ? BIT1_ON : BIT0_ON);
        return level_duration_make(false, bit ? BIT1_OFF : BIT0_OFF);
    }
    tx_state.repeat_pos++;
    if(tx_state.repeat_pos >= tx_state.repeats) { tx_state.sending = false; return level_duration_reset(); }
    tx_state.phase = 0; tx_state.bit_pos = 0;
    return level_duration_make(false, FRAME_GAP);
}

static int32_t tx_thread_worker(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    UNUSED(app);
    if(!g_device) return -1;
    FURI_LOG_I(TAG, "TX thread start");
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
    while(!subghz_devices_is_async_complete_tx(g_device)) furi_delay_ms(10);
    subghz_devices_stop_async_tx(g_device);
    subghz_devices_idle(g_device);
    subghz_devices_end(g_device);
    FURI_LOG_I(TAG, "TX thread done");
    return 0;
}

bool tx_init_hw(ProtoPirateApp* app, uint32_t freq) {
    if(!g_device) return false;
    FURI_LOG_I(TAG, "TX init: freq=%lu", freq);
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, freq);
    furi_delay_ms(10);
    UNUSED(app);
    return true;
}

void transmit_packet_nonblock(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep) {
    if(!app) return;
    if(g_tx_thread && furi_thread_get_state(g_tx_thread) == FuriThreadStateRunning) { return; }
    app->tx_data_hi = dhi; app->tx_data_lo = dlo; app->tx_freq = freq; app->tx_repeats = rep;
    g_tx_thread = furi_thread_alloc_ex("ProtoPirateTX", 2048, tx_thread_worker, app);
    furi_thread_start(g_tx_thread);
}

void transmit_packet_wait(ProtoPirateApp* app) {
    UNUSED(app);
    if(g_tx_thread) { furi_thread_join(g_tx_thread); furi_thread_free(g_tx_thread); g_tx_thread = NULL; }
}

void transmit_packet_stop(ProtoPirateApp* app) {
    UNUSED(app);
    tx_state.sending = false;
    if(g_device && !subghz_devices_is_async_complete_tx(g_device)) subghz_devices_stop_async_tx(g_device);
    if(g_device) subghz_devices_idle(g_device);
    if(g_tx_thread) { furi_thread_join(g_tx_thread); furi_thread_free(g_tx_thread); g_tx_thread = NULL; }
}

bool transmit_packet(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep) {
    if(!app) return false;
    transmit_packet_nonblock(app, dhi, dlo, freq, rep);
    transmit_packet_wait(app);
    return true;
}
