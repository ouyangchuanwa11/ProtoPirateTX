#include "protopirate_txrx.h"
#include "protopirate_radio.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../protocols/protocol_items.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

// RX worker callback - receives raw pulses
static void protopirate_rx_worker_callback(bool level, uint32_t duration, void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(app->decode_state->num_pulses < PROTOPIRATE_MAX_PULSES) {
        app->decode_state->pulses[app->decode_state->num_pulses++] = duration;
    }
}

// Start receiving
bool protopirate_rx_start(ProtoPirateApp* app) {
    furi_assert(app);

    if(app->rx_active) {
        protopirate_rx_stop(app);
    }

    // Allocate TX/RX worker
    app->txrx_worker = subghz_tx_rx_worker_alloc();
    if(!app->txrx_worker) return false;

    // Initialize radio
    app->radio_device = radio_device_loader_set(
        app->radio_device, SubGhzRadioDeviceTypeExternalCC1101);

    if(!app->radio_device) {
        // Fall back to internal
        app->radio_device = subghz_devices_get_by_name("cc1101_int");
    }

    if(!app->radio_device) return false;

    // Configure for RX
    subghz_devices_reset(app->radio_device);
    subghz_devices_load_preset(app->radio_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->radio_device, app->current_frequency);

    // Flush RX buffer
    subghz_devices_flush_rx(app->radio_device);

    // Start worker in RX mode
    if(!subghz_tx_rx_worker_start(app->txrx_worker, app->radio_device, app->current_frequency)) {
        subghz_tx_rx_worker_free(app->txrx_worker);
        app->txrx_worker = NULL;
        return false;
    }

    // Set callback for raw data
    subghz_tx_rx_worker_set_callback(app->txrx_worker, protopirate_rx_worker_callback, app);

    // Clear previous pulses
    app->decode_state->num_pulses = 0;
    app->decode_state->decoded = false;
    app->rx_active = true;

    return true;
}

// Stop receiving
void protopirate_rx_stop(ProtoPirateApp* app) {
    furi_assert(app);

    if(app->txrx_worker) {
        subghz_tx_rx_worker_stop(app->txrx_worker);
        subghz_tx_rx_worker_free(app->txrx_worker);
        app->txrx_worker = NULL;
    }

    app->rx_active = false;
}

// Start transmitting on a frequency
bool protopirate_tx_start(ProtoPirateApp* app, uint32_t frequency) {
    furi_assert(app);

    if(app->tx_active) {
        protopirate_tx_stop(app);
    }

    // Allocate worker
    app->txrx_worker = subghz_tx_rx_worker_alloc();
    if(!app->txrx_worker) return false;

    // Get radio device
    app->radio_device = radio_device_loader_set(
        app->radio_device, SubGhzRadioDeviceTypeExternalCC1101);

    if(!app->radio_device) {
        app->radio_device = subghz_devices_get_by_name("cc1101_int");
    }

    if(!app->radio_device) return false;

    // Configure for TX
    subghz_devices_reset(app->radio_device);
    subghz_devices_load_preset(app->radio_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->radio_device, frequency);

    app->tx_active = true;
    return true;
}

// Stop transmitting
void protopirate_tx_stop(ProtoPirateApp* app) {
    furi_assert(app);

    if(app->txrx_worker) {
        subghz_tx_rx_worker_stop(app->txrx_worker);
        subghz_tx_rx_worker_free(app->txrx_worker);
        app->txrx_worker = NULL;
    }

    if(app->radio_device) {
        subghz_devices_idle(app->radio_device);
    }

    app->tx_active = false;
}

// Transmit RAW data using subghz API
void protopirate_tx_data(ProtoPirateApp* app, uint32_t* pulses, uint32_t num_pulses, uint32_t frequency) {
    furi_assert(app);
    furi_assert(pulses);

    if(num_pulses == 0) return;

    // Use the subghz async TX API
    // Start TX mode
    if(!protopirate_tx_start(app, frequency)) return;

    // Convert pulse train to subghz format and transmit
    // subghz_devices_start_async_tx sends a sequence of level+duration pairs
    subghz_devices_start_async_tx(app->radio_device, NULL, NULL);

    uint32_t idx = 0;
    bool level = true; // Start with high

    while(idx < num_pulses) {
        uint32_t duration = pulses[idx];

        // Clamp duration to reasonable range
        if(duration < 50) duration = 50;
        if(duration > 100000) duration = 100000;

        // Async TX callback for each pulse
        subghz_devices_async_tx_yield(app->radio_device, level, duration);

        level = !level;
        idx++;
    }

    // End transmission
    subghz_devices_stop_async_tx(app->radio_device);

    // Clean up TX
    protopirate_tx_stop(app);
}

// Process received data through protocol decoders
bool protopirate_process_received_data(ProtoPirateApp* app) {
    furi_assert(app);

    if(!app->decode_state || app->decode_state->num_pulses == 0) return false;

    DecodeResult result;
    memset(&result, 0, sizeof(DecodeResult));
    result.frequency = app->current_frequency;

    // Try each protocol decoder
    bool decoded = protopirate_try_decode_all(
        app->decode_state->pulses,
        app->decode_state->num_pulses,
        &result);

    if(decoded && result.valid) {
        memcpy(&app->last_result, &result, sizeof(DecodeResult));
        app->decode_state->decoded = true;
        memcpy(&app->decode_state->result, &result, sizeof(DecodeResult));

        // Add to history
        protopirate_history_add(app->history, &result);
        app->need_saving = true;

        return true;
    }

    return false;
}
