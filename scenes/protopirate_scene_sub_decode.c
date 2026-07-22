#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../helpers/protopirate_txrx.h"
#include "../helpers/raw_file_reader.h"
#include "../protocols/protocol_items.h"

#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <lib/toolbox/path.h>
#include <storage/storage.h>

// File browser callback for selecting .sub files
static bool sub_file_select(ProtoPirateApp* app) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    FuriString* path = furi_string_alloc();
    furi_string_set(path, "/ext/subghz");

    DialogsFileBrowserOptions browser;
    dialog_file_browser_set_basic_options(&browser, ".sub", NULL);
    browser.hide_extensions = false;

    bool selected = dialog_file_browser_show(dialogs, path, path, &browser);

    if(selected) {
        furi_string_set(app->file_path, path);
        app->file_loaded = true;
    }

    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);
    return selected;
}

void protopirate_scene_sub_decode_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    // Open file browser
    if(sub_file_select(app)) {
        // Try to load and decode the .sub file
        DecodeResult result;
        if(protopirate_raw_file_decode(app->file_path, &result)) {
            memcpy(&app->last_result, &result, sizeof(DecodeResult));
            app->decode_state->decoded = true;
            app->decode_state->result = result;
            app->decode_state->result.valid = true;

            // Add to history
            protopirate_history_add(app->history, &result);

            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverInfo);
        } else {
            // Show error
            widget_reset(app->widget);
            widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary,
                                      "Decode Failed");
            widget_add_string_element(app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary,
                                      "Could not decode file");
            widget_add_string_element(app->widget, 64, 60, AlignCenter, AlignCenter, FontSecondary,
                                      "Press BACK to continue");
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdWidget);
        }
    } else {
        // User cancelled, go back
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeBack) {
        widget_reset(app->widget);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void protopirate_scene_sub_decode_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    widget_reset(app->widget);
    app->file_loaded = false;
}
