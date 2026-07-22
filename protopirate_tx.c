#include "protopirate_rb.h"

// ===================== TX ???? =====================
// TX????????,?????,?????
// ??Momentum????? furi_hal_subghz API

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

    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);

    for(uint8_t b = 0; b < repeats; b++) {
        furi_hal_subghz_tx();
        furi_delay_ms(250);
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
    UNUSED(data_hi);
    UNUSED(data_lo);

    FURI_LOG_I(TAG, "TX PACKET: hi=0x%08lX lo=0x%08lX freq=%lu repeats=%u",
               data_hi, data_lo, freq, repeats);

    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);
    furi_delay_ms(50);

    for(uint8_t b = 0; b < repeats; b++) {
        furi_hal_subghz_tx();
        furi_delay_ms(100);
        furi_hal_subghz_idle();
        furi_delay_ms(30);
    }

    FURI_LOG_I(TAG, "TX PACKET: complete");
    return true;
}

void transmit_start(ProtoPirateApp* app, uint32_t freq) {
    UNUSED(app);
    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);
    FURI_LOG_I(TAG, "TX START: freq=%lu", freq);
}

void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo) {
    UNUSED(data_hi);
    UNUSED(data_lo);
    UNUSED(app);
    furi_hal_subghz_tx();
    furi_delay_ms(80);
    furi_hal_subghz_idle();
    furi_delay_ms(30);
}

void transmit_stop(ProtoPirateApp* app) {
    UNUSED(app);
    furi_hal_subghz_idle();
    FURI_LOG_I(TAG, "TX STOP");
}
