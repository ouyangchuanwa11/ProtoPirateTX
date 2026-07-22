#include "protopirate_rb.h"

// ===================== TX ???? =====================
// TX????????,?????,?????
// ????Momentum???subghz_tx_rx_worker?furi_hal_subghz

/**
 * transmit_raw - ??RAW??
 * ????furi_hal_subghz??,?????
 */
bool transmit_raw(ProtoPirateApp* app, FuriString* raw_data, uint32_t freq, uint8_t repeats) {
    if(!app || !raw_data || furi_string_empty(raw_data)) {
        FURI_LOG_E(TAG, "transmit_raw: invalid params");
        return false;
    }

    FURI_LOG_I(TAG, "TX RAW: freq=%lu, repeats=%u", freq, repeats);

    // ????
    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);

    // ??????
    SubGhzProtocolDecoderRAW* decoder_raw = malloc(sizeof(SubGhzProtocolDecoderRAW));
    SubGhzProtocolDecoderRAW_alloc(decoder_raw, app->environment);
    decoder_raw->protocol = subghz_protocol_raw_get_protocol();

    // ??RAW??
    if(!subghz_protocol_raw_parse_from_str(
           decoder_raw, furi_string_get_cstr(raw_data))) {
        FURI_LOG_E(TAG, "TX RAW: parse failed");
        free(decoder_raw);
        return false;
    }

    // ??!- ????,???
    for(uint8_t b = 0; b < repeats; b++) {
        furi_hal_subghz_tx(decoder_raw->parser_data, true);
        // ????250ms???
        furi_delay_ms(200);
        furi_hal_subghz_stop();
        furi_delay_ms(50);
    }

    free(decoder_raw);
    FURI_LOG_I(TAG, "TX RAW: complete (%u bursts)", repeats);
    return true;
}

/**
 * transmit_packet - ?????(??????)
 * ????furi_hal_subghz????
 */
bool transmit_packet(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo, uint32_t freq, uint8_t repeats) {
    if(!app) return false;

    FURI_LOG_I(TAG, "TX PACKET: hi=0x%08lX lo=0x%08lX freq=%lu repeats=%u",
               data_hi, data_lo, freq, repeats);

    // [TX?? - ????]
    // ???????
    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);

    // ??????
    uint8_t bit_buf[16];
    uint8_t byte_count = 0;
    bit_buf[byte_count++] = (data_hi >> 24) & 0xFF;
    bit_buf[byte_count++] = (data_hi >> 16) & 0xFF;
    bit_buf[byte_count++] = (data_hi >> 8) & 0xFF;
    bit_buf[byte_count++] = data_hi & 0xFF;
    bit_buf[byte_count++] = (data_lo >> 24) & 0xFF;
    bit_buf[byte_count++] = (data_lo >> 16) & 0xFF;
    bit_buf[byte_count++] = (data_lo >> 8) & 0xFF;
    bit_buf[byte_count++] = data_lo & 0xFF;

    // ?? - ??????????
    for(uint8_t b = 0; b < repeats; b++) {
        // ??????subghz??
        furi_hal_subghz_start_async_tx(bit_buf, byte_count * 8);
        furi_delay_ms(250);
        furi_hal_subghz_stop_async_tx();
        furi_hal_subghz_idle();
        furi_delay_ms(50);
    }

    FURI_LOG_I(TAG, "TX PACKET: complete");
    return true;
}

/**
 * transmit_start - ??????
 * ??RollBack????????
 */
void transmit_start(ProtoPirateApp* app, uint32_t freq) {
    furi_hal_subghz_set_path(FuriHalSubGhzPathIsolate);
    furi_hal_subghz_reset();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_subghz_set_frequency(freq);

    FURI_LOG_I(TAG, "TX START: freq=%lu", freq);
}

/**
 * transmit_burst - ??????
 */
void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo) {
    uint8_t buf[8];
    buf[0] = (data_hi >> 24) & 0xFF;
    buf[1] = (data_hi >> 16) & 0xFF;
    buf[2] = (data_hi >> 8) & 0xFF;
    buf[3] = data_hi & 0xFF;
    buf[4] = (data_lo >> 24) & 0xFF;
    buf[5] = (data_lo >> 16) & 0xFF;
    buf[6] = (data_lo >> 8) & 0xFF;
    buf[7] = data_lo & 0xFF;

    furi_hal_subghz_start_async_tx(buf, 64);
    furi_delay_ms(200);
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
}

/**
 * transmit_stop - ????
 */
void transmit_stop(ProtoPirateApp* app) {
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    FURI_LOG_I(TAG, "TX STOP");
}
