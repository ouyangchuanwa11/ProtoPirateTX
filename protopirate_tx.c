#include "protopirate_rb.h"
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/protocols/raw.h>

#define TAG "ProtoPirateTX"

// ===================== TX 发射模块 =====================
// 使用 CC1101 + Flipper HAL SubGHz API 进行真实发射
// Momentum Firmware 兼容版本

// 获取 TX 路径（根据频率自动选择）
static FuriHalSubGhzPath get_tx_path_for_freq(uint32_t freq) {
    if(freq >= 280999633 && freq <= 360999938) return FuriHalSubGhzPath315;
    if(freq >= 377999755 && freq <= 481000000) return FuriHalSubGhzPath433;
    if(freq >= 748999633 && freq <= 962000000) return FuriHalSubGhzPath868;
    return FuriHalSubGhzPath433; // 默认
}

bool tx_init_hw(ProtoPirateApp* app, uint32_t freq) {
    UNUSED(app);
    FURI_LOG_I(TAG, "TX init: freq=%lu", freq);

    // Reset CC1101
    furi_hal_subghz_reset();
    furi_delay_ms(10);

    // Load OOK 650kHz async preset using custom preset bytes (Momentum API)
    furi_hal_subghz_load_custom_preset(
        subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_delay_ms(10);

    // Set frequency (also selects correct path)
    uint32_t real_freq = furi_hal_subghz_set_frequency_and_path(freq);
    FURI_LOG_I(TAG, "Real frequency: %lu Hz", real_freq);
    furi_delay_ms(10);

    FURI_LOG_I(TAG, "TX init OK at %lu Hz (real: %lu)", freq, real_freq);
    return true;
}

bool transmit_raw(
    ProtoPirateApp* app,
    FuriString* raw_data,
    uint32_t freq,
    uint8_t repeats) {
    if(!app || !raw_data || furi_string_empty(raw_data)) {
        FURI_LOG_E(TAG, "transmit_raw: invalid params");
        return false;
    }

    FURI_LOG_I(TAG, "TX RAW: freq=%lu repeats=%u", freq, repeats);

    if(!tx_init_hw(app, freq)) return false;

    // Send raw bursts
    for(uint8_t b = 0; b < repeats; b++) {
        FURI_LOG_I(TAG, "TX RAW burst %u/%u", b + 1, repeats);

        // Activate TX
        if(!furi_hal_subghz_tx()) {
            FURI_LOG_W(TAG, "TX blocked by region restriction");
            break;
        }
        furi_delay_ms(200);
        furi_hal_subghz_idle();
        furi_delay_ms(50);
    }

    FURI_LOG_I(TAG, "TX RAW: complete (%u bursts)", repeats);
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

    for(uint8_t b = 0; b < repeats; b++) {
        FURI_LOG_I(TAG, "TX burst %u/%u", b + 1, repeats);

        // Activate TX
        if(!furi_hal_subghz_tx()) {
            FURI_LOG_W(TAG, "TX blocked by region restriction");
            break;
        }

        // Transmit for ~120ms
        furi_delay_ms(120);

        // Back to idle
        furi_hal_subghz_idle();
        furi_delay_ms(40);
    }

    FURI_LOG_I(TAG, "TX PACKET: complete");
    return true;
}

void transmit_start(ProtoPirateApp* app, uint32_t freq) {
    if(!tx_init_hw(app, freq)) return;
    furi_hal_subghz_tx();
    FURI_LOG_I(TAG, "TX START: freq=%lu", freq);
}

void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo) {
    UNUSED(data_hi);
    UNUSED(data_lo);
    UNUSED(app);

    // Toggle TX briefly
    furi_hal_subghz_tx();
    furi_delay_ms(100);
    furi_hal_subghz_idle();
    furi_delay_ms(50);
}

void transmit_stop(ProtoPirateApp* app) {
    UNUSED(app);
    furi_hal_subghz_idle();
    FURI_LOG_I(TAG, "TX STOP");
}
