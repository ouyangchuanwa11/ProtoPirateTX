#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../helpers/protopirate_storage.h"

#include <furi.h>
#include <gui/modules/submenu.h>

void protopirate_scene_saved_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    submenu_reset(app->submenu);

    // Load saved signals from storage
    protopirate_storage_load_saved(app);

    if(app->saved_count == 0) {
        submenu_add_item(app->submenu, "No saved signals", 99, NULL, NULL);
        submenu_add_item(app->submenu, "Back", 0, NULL, NULL);
    } else {
        for(uint32_t i = 0; i < app->saved_count; i++) {
            submenu_add_item(
                app->submenu, app->saved_signals[i].name, i + 100, NULL, NULL);
        }
        submenu_add_item(app->submenu, "Back", 99, NULL, NULL);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSubmenu);
}

bool protopirate_scene_saved_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event >= 100 && event.event < (100 + app->saved_count)) {
            uint32_t index = event.event - 100;
            if(index < app->saved_count) {
                // Load the saved signal into last_result
                memcpy(&app->last_result, &app->saved_signals[index].result, sizeof(DecodeResult));
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSavedInfo);
            }
            return true;
        }
        if(event.event == 99) {
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

void protopirate_scene_saved_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    submenu_reset(app->submenu);
}
