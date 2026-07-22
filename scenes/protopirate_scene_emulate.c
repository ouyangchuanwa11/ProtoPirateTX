#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../helpers/protopirate_txrx.h"
#include "../helpers/protopirate_radio.h"
#include "../helpers/protopirate_storage.h"
#include "../protocols/protocol_items.h"

#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <lib/subghz/subghz_tx_rx_worker.h>

static void protopirate_emulate_do_tx(ProtoPirateApp* app, int32_t rollback) {
    // Copy the last result and apply rollback if needed
    DecodeResult tx_result;
    memcpy(&tx_result, &app->last_result, sizeof(DecodeResult));

    // Apply rollback to counter for protocols that support it
    if(rollback != 0 && (tx_result.protocol->features & PROTO_HAS_ROLLBACK)) {
        // Roll back the counter
        if(tx_result.counter > abs(rollback)) {
            tx_result.counter -= abs(rollback);
        } else {
            tx_result.counter = 0;
        }

        // Recalculate CRC based on protocol type
        // Each protocol's CRC is calculated differently
        switch(tx_result.protocol->manufacturer) {
        case ManufacturerKia:
            // KeeLoq-based: CRC is encrypted counter
            // For Kia: use the standard KeeLoq decrypt/encrypt
            tx_result.crc = protopirate_calc_keeloq_crc(
                tx_result.serial, tx_result.button, tx_result.counter);
            break;
        case ManufacturerFord:
            // Ford uses a simple checksum
            tx_result.crc = protopirate_calc_ford_crc(
                tx_result.serial, tx_result.button, tx_result.counter);
            break;
        case ManufacturerHonda:
            // Honda uses XOR-based checksum
            tx_result.crc = protopirate_calc_honda_crc(
                tx_result.serial, tx_result.button, tx_result.counter);
            break;
        default:
            // For other protocols, keep original CRC
            break;
        }
    }

    // Build raw waveform from protocol data
    uint32_t pulse_buffer[PROTOPIRATE_MAX_PULSES];
    uint32_t pulse_count = 0;

    // Generate appropriate waveform based on protocol
    if(tx_result.protocol->features & PROTO_KEE_LOQ) {
        // KeeLoq protocol: Manchester encoded
        pulse_count = protopirate_build_keeloq_waveform(
            &tx_result, pulse_buffer, PROTOPIRATE_MAX_PULSES);
    } else if(tx_result.protocol->features & PROTO_HAS_FIXED) {
        // Fixed code: simple PWM
        pulse_count = protopirate_build_fixed_waveform(
            &tx_result, pulse_buffer, PROTOPIRATE_MAX_PULSES);
    } else {
        // Other: use raw data if available
        if(tx_result.raw_data && tx_result.raw_len > 0) {
            memcpy(pulse_buffer, tx_result.raw_data,
                   MIN(tx_result.raw_len, PROTOPIRATE_MAX_PULSES * sizeof(uint32_t)));
            pulse_count = tx_result.raw_len / sizeof(uint32_t);
        }
    }

    if(pulse_count > 0) {
        // Transmit pulses
        protopirate_tx_data(app, pulse_buffer, pulse_count, tx_result.frequency);
    }
}

// TX submenu callback items
typedef enum {
    EmulateSubmenuIndexTX,
    EmulateSubmenuIndexRollback1,
    EmulateSubmenuIndexRollback10,
    EmulateSubmenuIndexRollback100,
    EmulateSubmenuIndexSave,
    EmulateSubmenuIndexCancel,
    EmulateSubmenuIndexCount,
} EmulateSubmenuIndex;

void protopirate_scene_emulate_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    submenu_reset(app->submenu);

    char buf[64];

    // Show protocol info as header
    if(app->last_result.valid) {
        snprintf(buf, sizeof(buf), "TX: %s", app->last_result.protocol->display_name);
        submenu_add_item(app->submenu, buf, EmulateSubmenuIndexTX, NULL, NULL);

        // Add separator item
        submenu_add_item(app->submenu, "---", 99, NULL, NULL);

        // TX options
        submenu_add_item(app->submenu, "TX (Rollback 0)", EmulateSubmenuIndexTX, NULL, NULL);

        // Rollback options (only if protocol supports it)
        if(app->last_result.protocol->features & PROTO_HAS_ROLLBACK) {
            submenu_add_item(app->submenu, "TX Rollback -1", EmulateSubmenuIndexRollback1, NULL, NULL);
            submenu_add_item(app->submenu, "TX Rollback -10", EmulateSubmenuIndexRollback10, NULL, NULL);
            submenu_add_item(app->submenu, "TX Rollback -100", EmulateSubmenuIndexRollback100, NULL, NULL);
        }

        submenu_add_item(app->submenu, "Save Signal", EmulateSubmenuIndexSave, NULL, NULL);
        submenu_add_item(app->submenu, "Cancel", EmulateSubmenuIndexCancel, NULL, NULL);
    } else {
        submenu_add_item(app->submenu, "No signal data", 99, NULL, NULL);
        submenu_add_item(app->submenu, "Back", EmulateSubmenuIndexCancel, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSubmenu);
}

bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EmulateSubmenuIndexTX:
            // TX with no rollback
            app->rollback_amount = 0;
            app->rollback_enabled = false;

            // Show TX popup
            popup_reset(app->popup);
            popup_set_header(app->popup, "Transmitting...", 64, 32, AlignCenter, AlignCenter);
            popup_set_text(app->popup, "Hold remote near Flipper", 64, 50, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPopup);
            furi_timer_pending(NULL);

            // Do the TX - repeat 3 times for reliability
            for(int i = 0; i < 3; i++) {
                protopirate_emulate_do_tx(app, 0);
                furi_delay_ms(50);
            }

            // Wait and go back
            furi_delay_ms(100);
            popup_reset(app->popup);
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case EmulateSubmenuIndexRollback1:
            app->rollback_amount = -1;
            app->rollback_enabled = true;

            popup_reset(app->popup);
            popup_set_header(app->popup, "TX Rollback -1", 64, 32, AlignCenter, AlignCenter);
            popup_set_text(app->popup, "Transmitting with -1 rollback", 64, 50, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPopup);

            for(int i = 0; i < 3; i++) {
                protopirate_emulate_do_tx(app, 1);
                furi_delay_ms(50);
            }

            furi_delay_ms(100);
            popup_reset(app->popup);
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case EmulateSubmenuIndexRollback10:
            app->rollback_amount = -10;
            app->rollback_enabled = true;

            popup_reset(app->popup);
            popup_set_header(app->popup, "TX Rollback -10", 64, 32, AlignCenter, AlignCenter);
            popup_set_text(app->popup, "Transmitting with -10 rollback", 64, 50, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPopup);

            for(int i = 0; i < 3; i++) {
                protopirate_emulate_do_tx(app, 10);
                furi_delay_ms(50);
            }

            furi_delay_ms(100);
            popup_reset(app->popup);
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case EmulateSubmenuIndexRollback100:
            app->rollback_amount = -100;
            app->rollback_enabled = true;

            popup_reset(app->popup);
            popup_set_header(app->popup, "TX Rollback -100", 64, 32, AlignCenter, AlignCenter);
            popup_set_text(app->popup, "Transmitting with -100 rollback", 64, 50, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPopup);

            for(int i = 0; i < 3; i++) {
                protopirate_emulate_do_tx(app, 100);
                furi_delay_ms(50);
            }

            furi_delay_ms(100);
            popup_reset(app->popup);
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case EmulateSubmenuIndexSave:
            // Save the signal
            protopirate_storage_save_signal(app);
            popup_reset(app->popup);
            popup_set_header(app->popup, "Saved!", 64, 32, AlignCenter, AlignCenter);
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPopup);
            furi_delay_ms(500);
            popup_reset(app->popup);
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case EmulateSubmenuIndexCancel:
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void protopirate_scene_emulate_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    submenu_reset(app->submenu);
    popup_reset(app->popup);
}
