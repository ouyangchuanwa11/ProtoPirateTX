#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

void protopirate_scene_need_saving_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 15, AlignCenter, AlignCenter, FontPrimary,
                              "Unsaved Data!");
    widget_add_string_element(app->widget, 64, 35, AlignCenter, AlignCenter, FontSecondary,
                              "You have unsaved signals");
    widget_add_string_element(app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary,
                              "OK: Save and Exit");
    widget_add_string_element(app->widget, 64, 62, AlignCenter, AlignCenter, FontSecondary,
                              "BACK: Discard and Exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdWidget);
}

bool protopirate_scene_need_saving_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventNeedSavingYes:
            // Save and exit
            app->need_saving = false;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, ProtoPirateSceneStart);
            return true;
        case ProtoPirateCustomEventNeedSavingNo:
            // Discard and exit
            app->need_saving = false;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, ProtoPirateSceneStart);
            return true;
        case ProtoPirateCustomEventNeedSavingCancel:
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        // Discard and go back
        app->need_saving = false;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, ProtoPirateSceneStart);
        return true;
    }

    return false;
}

void protopirate_scene_need_saving_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    widget_reset(app->widget);
}
