#pragma once

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// Settings structure
typedef struct {
    uint32_t last_frequency;
    uint32_t last_band;
    float timing_factor;
    bool save_raw_data;
    bool auto_detect_protocol;
    uint32_t tx_repeat_count;
    uint32_t rx_timeout_ms;
} ProtoPirateSettings;

// Default settings
#define PROTOPIRATE_SETTINGS_DEFAULT \
    (ProtoPirateSettings) { \
        .last_frequency = 433920000, \
        .last_band = 1, \
        .timing_factor = 1.0f, \
        .save_raw_data = false, \
        .auto_detect_protocol = true, \
        .tx_repeat_count = 3, \
        .rx_timeout_ms = 5000, \
    }

// Settings functions
void protopirate_settings_load(ProtoPirateSettings* settings);
void protopirate_settings_save(ProtoPirateSettings* settings);
void protopirate_settings_defaults(ProtoPirateSettings* settings);
