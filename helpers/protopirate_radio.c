#include "protopirate_radio.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

// Initialize radio device
SubGhzRadioDevice* protopirate_radio_init(uint32_t frequency) {
    SubGhzRadioDevice* device = NULL;

    // Try external CC1101 first
    device = subghz_devices_get_by_name("cc1101_ext");

    if(!device) {
        // Fall back to internal
        device = subghz_devices_get_by_name("cc1101_int");
    }

    if(!device) return NULL;

    // Reset and configure
    subghz_devices_reset(device);
    subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(device, frequency);

    return device;
}

// Deinitialize radio
void protopirate_radio_deinit(SubGhzRadioDevice* device) {
    if(!device) return;
    subghz_devices_idle(device);
}

// Set frequency
bool protopirate_radio_set_frequency(SubGhzRadioDevice* device, uint32_t frequency) {
    if(!device) return false;
    return subghz_devices_set_frequency(device, frequency);
}

// Start RX
bool protopirate_radio_start_rx(SubGhzRadioDevice* device) {
    if(!device) return false;
    subghz_devices_flush_rx(device);
    return true;
}

// Stop RX
void protopirate_radio_stop_rx(SubGhzRadioDevice* device) {
    if(!device) return;
    subghz_devices_idle(device);
}

// Start TX
bool protopirate_radio_start_tx(SubGhzRadioDevice* device) {
    if(!device) return false;
    subghz_devices_start_async_tx(device, NULL, NULL);
    return true;
}

// Stop TX
void protopirate_radio_stop_tx(SubGhzRadioDevice* device) {
    if(!device) return;
    subghz_devices_stop_async_tx(device);
    subghz_devices_idle(device);
}

// Send raw pulse data
void protopirate_radio_send_raw(SubGhzRadioDevice* device, uint32_t* pulses, uint32_t num_pulses) {
    if(!device || !pulses || num_pulses == 0) return;

    subghz_devices_start_async_tx(device, NULL, NULL);

    bool level = true;
    for(uint32_t i = 0; i < num_pulses; i++) {
        uint32_t duration = pulses[i];
        if(duration < 50) duration = 50;
        if(duration > 100000) duration = 100000;
        subghz_devices_async_tx_yield(device, level, duration);
        level = !level;
    }

    subghz_devices_stop_async_tx(device);
    subghz_devices_idle(device);
}
