#include "protopirate_rb.h"
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/protocols/raw.h>
#include <furi.h>

#undef TAG
#define TAG "ProtoPirateTX"

#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"

typedef struct {
    uint32_t data[2];
    uint8_t bit_pos;
    uint8_t byte_pos;
    uint32_t repeat_pos;
    uint8_t repeats;
    uint8_t phase;
    bool sending;
} TxState;

static TxState tx_state;
static FuriThread* g_tx_thread = NULL;
static SubGhzDevice* g_device = NULL;

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
    if(tx_state.phase == 1) { tx_state.phase = 2; tx_state.bit_pos = 0; tx_state.byte_pos = 0; return level_duration_make(false, PRE_OFF); }
    if(tx_state.bit_pos < 64) {
        uint32_t word = (tx_state.bit_pos < 32) ? tx_state.data[0] : tx_state.data[1];
        uint8_t bit_offset = (tx_state.bit_pos < 32) ? (31 - tx_state.bit_pos) : (63 - tx_state.bit_pos);
        uint8_t bit = (word >> bit_offset) & 1;
        if(tx_state.byte_pos == 0) { tx_state.byte_pos = 1; return level_duration_make(true, bit ? BIT1_ON : BIT0_ON); }
        else { tx_state.byte_pos = 0; tx_state.bit_pos++; return level_duration_make(false, bit ? BIT1_OFF : BIT0_OFF); }
    }
    tx_state.repeat_pos++;
    if(tx_state.repeat_pos >= tx_state.repeats) { tx_state.sending = false; return level_duration_reset(); }
    tx_state.phase = 0; tx_state.byte_pos = 0;
    return level_duration_make(false, FRAME_GAP);
}

static SubGhzDevice* tx_get_device(void) {
    if(!g_device) {
        g_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        if(!g_device) FURI_LOG_E(TAG, "Failed to get CC1101 device");
    }
    return g_device;
}

static int32_t tx_thread_worker(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return -1;
    SubGhzDevice* dev = tx_get_device();
    if(!dev) return -1;
    FURI_LOG_I(TAG, "TX thread start (subghz_devices)");
    subghz_devices_begin(dev);
    subghz_devices_set_preset(dev, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(dev, app->tx_freq);
    subghz_devices_set_tx(dev);
    furi_delay_ms(50);
    tx_state.data[0] = app->tx_data_hi;
    tx_state.data[1] = app->tx_data_lo;
    tx_state.repeat_pos = 0;
    tx_state.repeats = app->tx_repeats;
    tx_state.phase = 0;
    tx_state.byte_pos = 0;
    tx_state.bit_pos = 0;
    tx_state.sending = true;
    FURI_LOG_I(TAG, "TX starting: hi=0x%08lX lo=0x%08lX freq=%lu reps=%u", app->tx_data_hi, app->tx_data_lo, app->tx_freq, app->tx_repeats);
    subghz_devices_start_async_tx(dev, tx_callback, NULL);
    while(!subghz_devices_is_async_tx_complete(dev)) furi_delay_ms(10);
    subghz_devices_stop_async_tx(dev);
    subghz_devices_idle(dev);
    subghz_devices_end(dev);
    FURI_LOG_I(TAG, "TX thread complete");
    return 0;
}

bool tx_init_hw(ProtoPirateApp* app, uint32_t freq) {
    SubGhzDevice* dev = tx_get_device();
    if(!dev) return false;
    FURI_LOG_I(TAG, "TX init: freq=%lu", freq);
    subghz_devices_begin(dev);
    subghz_devices_set_preset(dev, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(dev, freq);
    furi_delay_ms(10);
    return true;
}

void transmit_packet_nonblock(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo, uint32_t freq, uint8_t repeats) {
    if(!app) return;
    if(g_tx_thread && furi_thread_get_state(g_tx_thread) == FuriThreadStateRunning) { FURI_LOG_W(TAG, "TX already in progress"); return; }
    app->tx_data_hi = data_hi; app->tx_data_lo = data_lo; app->tx_freq = freq; app->tx_repeats = repeats;
    FURI_LOG_I(TAG, "TX NONBLOCK: hi=0x%08lX lo=0x%08lX freq=%lu reps=%u", data_hi, data_lo, freq, repeats);
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
    SubGhzDevice* dev = tx_get_device();
    if(dev) { if(!subghz_devices_is_async_tx_complete(dev)) subghz_devices_stop_async_tx(dev); subghz_devices_idle(dev); }
    if(g_tx_thread) { furi_thread_join(g_tx_thread); furi_thread_free(g_tx_thread); g_tx_thread = NULL; }
}

bool transmit_packet(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo, uint32_t freq, uint8_t repeats) {
    if(!app) return false;
    transmit_packet_nonblock(app, data_hi, data_lo, freq, repeats);
    transmit_packet_wait(app);
    return true;
}
