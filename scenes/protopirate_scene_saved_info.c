#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../helpers/protopirate_txrx.h"

#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

void protopirate_scene_saved_info_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    submenu_reset(app->submenu);

    char buf[64];

    if(app->last_result.valid) {
        submenu_add_item(app->submenu, app->last_result.protocol->display_name, 0, NULL, NULL);

        snprintf(buf, sizeof(buf), "Serial: %08lX", app->last_result.serial);
        submenu_add_item(app->submenu, buf, 1, NULL, NULL);

        snprintf(buf, sizeof(buf), "Button: %lu", app->last_result.button);
        submenu_add_item(app->submenu, buf, 2, NULL, NULL);

        snprintf(buf, sizeof(buf), "Counter: %lu", app->last_result.counter);
        submenu_add_item(app->submenu, buf, 3, NULL, NULL);

        submenu_add_item(app->submenu, "---", 99, NULL, NULL);
        submenu_add_item(app->submenu, "TX / Emulate", ProtoPirateCustomEventEmulateTX, NULL, NULL);
        submenu_add_item(app->submenu, "Delete", ProtoPirateCustomEventSavedDelete, NULL, NULL);
        submenu_add_item(app->submenu, "Back", 0, NULL, NULL);
    } else {
        submenu_add_item(app->submenu, "No signal data", 0, NULL, NULL);
        submenu_add_item(app->submenu, "Back", 0, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSubmenu);
}

bool protopirate_scene_saved_info_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventEmulateTX:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
            return true;
        case ProtoPirateCustomEventSavedDelete:
            // Delete the current signal
            if(app->last_result.valid) {
                // Find and remove from saved list
                for(uint32_t i = 0; i < app->saved_count; i++) {
                    if(app->saved_signals[i].result.serial == app->last_result.serial &&
                       app->saved_signals[i].result.protocol == app->last_result.protocol) {
                        // Shift remaining entries
                        memmove(&app->saved_signals[i], &app->saved_signals[i + 1],
                                (app->saved_count - i - 1) * sizeof(SavedSignal));
                        app->saved_count--;
                        memset(&app->last_result, 0, sizeof(DecodeResult));
                        break;
                    }
                }
                protopirate_storage_save_all(app);
            }
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

void protopirate_scene_saved_info_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
