#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../protopirate_history.h"
#include "../defines.h"
#include "../helpers/protopirate_txrx.h"
#include "../helpers/protopirate_radio.h"
#include "../protocols/protocol_items.h"

#include <furi.h>
#include <gui/view.h>

// Forward declare the view update
static void protopirate_scene_receiver_update(ProtoPirateApp* app);

void protopirate_scene_receiver_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    // Clear any previous decode state
    app->decode_state->decoded = false;
    app->decode_state->num_pulses = 0;
    memset(&app->decode_state->result, 0, sizeof(DecodeResult));
    memset(&app->last_result, 0, sizeof(DecodeResult));

    // Start RX
    protopirate_rx_start(app);

    // Show the receiver view
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdReceiver);
}

bool protopirate_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventReceiverNewData:
            // Incoming data from RX worker - try to decode
            protopirate_process_received_data(app);

            // Update view
            protopirate_scene_receiver_update(app);
            return true;

        case ProtoPirateCustomEventReceiverOk:
            // User pressed OK on a decoded result - go to emulate
            if(app->last_result.valid) {
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
            }
            return true;

        case ProtoPirateCustomEventReceiverConfig:
            // User wants to configure frequency
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeTick) {
        // Periodic update - check for new data
        protopirate_scene_receiver_update(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(app->need_saving) {
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneNeedSaving);
            return true;
        }
        // Stop RX
        protopirate_rx_stop(app);
        return false;
    }

    return false;
}

void protopirate_scene_receiver_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    protopirate_rx_stop(app);
}

static void protopirate_scene_receiver_update(ProtoPirateApp* app) {
    // Update the view model with latest decode state
    DecodeState* model = view_get_model(app->receiver_view);
    if(model) {
        memcpy(model, app->decode_state, sizeof(DecodeState));
        view_commit_model(app->receiver_view, true);
    }
}
