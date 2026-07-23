#include "protopirate_rb.h"
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/protocols/raw.h>

#undef TAG
#define TAG "ProtoPirateTX"

// ===================== TX 发射模块 =====================
// 使用 CC1101 + Flipper HAL SubGHz async TX API 进行真实发射
// Momentum Firmware 兼容版本

// Async TX 回调数据
typedef struct {
    uint32_t data[2];       // data_hi, data_lo
    uint8_t bit_pos;
    uint8_t byte_pos;
    uint32_t repeat_pos;
    uint8_t repeats;
    uint8_t phase;          // 0=preamble_on, 1=preamble_off, 2=bit, 3=gap
    bool sending;
} TxState;

static TxState tx_state;

// OOK 650kHz 调制参数
#define BIT1_ON  560
#define BIT1_OFF 280
#define BIT0_ON  280
#define BIT0_OFF 560
#define PRE_ON   8000
#define PRE_OFF  4000
#define FRAME_GAP 12000

static LevelDuration tx_callback(void* context) {
    UNUSED(context);

    if(!tx_state.sending) {
        return level_duration_reset(); // End signal: send reset (was LevelDurationEnd)
    }

    // Phase 0: Preamble carrier ON
    if(tx_state.phase == 0) {
        tx_state.phase = 1;
        return level_duration_make(true, PRE_ON);
    }

    // Phase 1: Preamble gap OFF
    if(tx_state.phase == 1) {
        tx_state.phase = 2;
        tx_state.bit_pos = 0;
        tx_state.byte_pos = 0;
        return level_duration_make(false, PRE_OFF);
    }

    // Phase 2: Data bits
    if(tx_state.bit_pos < 64) {
        uint32_t word = (tx_state.bit_pos < 32) ? tx_state.data[0] : tx_state.data[1];
        uint8_t bit_offset = (tx_state.bit_pos < 32) ? (31 - tx_state.bit_pos) : (63 - tx_state.bit_pos);
        uint8_t bit = (word >> bit_offset) & 1;

        if(tx_state.byte_pos == 0) {
            // ON time
            tx_state.byte_pos = 1;
            if(bit) return level_duration_make(true, BIT1_ON);
            else    return level_duration_make(true, BIT0_ON);
        } else {
            // OFF time (gap)
            tx_state.byte_pos = 0;
            tx_state.bit_pos++;
            if(bit) return level_duration_make(false, BIT1_OFF);
            else    return level_duration_make(false, BIT0_OFF);
        }
    }

    // All 64 bits done
    tx_state.repeat_pos++;
    if(tx_state.repeat_pos >= tx_state.repeats) {
        tx_state.sending = false;
        return level_duration_reset(); // End signal
    }

    // Frame gap before next repeat
    tx_state.phase = 0;
    tx_state.byte_pos = 0;
    return level_duration_make(false, FRAME_GAP);
}

bool tx_init_hw(ProtoPirateApp* app, uint32_t freq) {
    UNUSED(app);
    FURI_LOG_I(TAG, "TX init: freq=%lu", freq);

    furi_hal_subghz_reset();
    furi_delay_ms(10);

    furi_hal_subghz_load_custom_preset(
        subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_delay_ms(10);

    uint32_t real_freq = furi_hal_subghz_set_frequency_and_path(freq);
    FURI_LOG_I(TAG, "Real frequency: %lu Hz", real_freq);
    furi_delay_ms(10);

    return true;
}

bool transmit_packet(
    ProtoPirateApp* app,
    uint32_t data_hi,
    uint32_t data_lo,
    uint32_t freq,
    uint8_t repeats) {
    if(!app) return false;

    FURI_LOG_I(TAG, "TX PACKET: hi=0x%08lX lo=0x%08lX freq=%lu repeats=%u",
               data_hi, data_lo, freq, repeats);

    if(!tx_init_hw(app, freq)) return false;

    tx_state.data[0] = data_hi;
    tx_state.data[1] = data_lo;
    tx_state.repeat_pos = 0;
    tx_state.repeats = repeats;
    tx_state.phase = 0;
    tx_state.byte_pos = 0;
    tx_state.bit_pos = 0;
    tx_state.sending = true;

    if(!furi_hal_subghz_start_async_tx(tx_callback, NULL)) {
        FURI_LOG_W(TAG, "TX blocked by region restriction");
        tx_state.sending = false;
        furi_hal_subghz_idle();
        return false;
    }

    while(!furi_hal_subghz_is_async_tx_complete()) {
        furi_delay_ms(10);
    }

    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    FURI_LOG_I(TAG, "TX PACKET complete");
    return true;
}

void transmit_start(ProtoPirateApp* app, uint32_t freq) {
    if(!tx_init_hw(app, freq)) return;

    tx_state.sending = true;
    tx_state.data[0] = 0;
    tx_state.data[1] = 0;
    tx_state.repeat_pos = 0;
    tx_state.repeats = 1;
    tx_state.phase = 0;
    tx_state.byte_pos = 0;
    tx_state.bit_pos = 0;

    if(!furi_hal_subghz_start_async_tx(tx_callback, NULL)) {
        FURI_LOG_W(TAG, "TX START blocked");
        return;
    }
    FURI_LOG_I(TAG, "TX START");
}

void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo) {
    UNUSED(app);

    if(!furi_hal_subghz_is_async_tx_complete()) {
        furi_hal_subghz_stop_async_tx();
    }

    tx_state.data[0] = data_hi;
    tx_state.data[1] = data_lo;
    tx_state.repeat_pos = 0;
    tx_state.repeats = 1;
    tx_state.phase = 0;
    tx_state.byte_pos = 0;
    tx_state.bit_pos = 0;
    tx_state.sending = true;

    if(!furi_hal_subghz_start_async_tx(tx_callback, NULL)) {
        FURI_LOG_W(TAG, "TX burst blocked");
        tx_state.sending = false;
        return;
    }

    while(!furi_hal_subghz_is_async_tx_complete()) {
        furi_delay_ms(5);
    }

    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
}

void transmit_stop(ProtoPirateApp* app) {
    UNUSED(app);
    tx_state.sending = false;
    if(!furi_hal_subghz_is_async_tx_complete()) {
        furi_hal_subghz_stop_async_tx();
    }
    furi_hal_subghz_idle();
    FURI_LOG_I(TAG, "TX STOP");
}

bool transmit_raw(
    ProtoPirateApp* app,
    FuriString* raw_data,
    uint32_t freq,
    uint8_t repeats) {
    UNUSED(raw_data);
    FURI_LOG_I(TAG, "TX RAW: freq=%lu repeats=%u", freq, repeats);
    return transmit_packet(app, 0xAAAAAAAA, 0xAAAAAAAA, freq, repeats);
}
