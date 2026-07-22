#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

void protopirate_scene_start_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu, "Receive", ProtoPirateCustomEventStartReceive, NULL, NULL);
    submenu_add_item(
        app->submenu, "Saved Signals", ProtoPirateCustomEventStartSaved, NULL, NULL);
    submenu_add_item(
        app->submenu, "Sub Decode", ProtoPirateCustomEventStartSubDecode, NULL, NULL);
    submenu_add_item(
        app->submenu, "About", ProtoPirateCustomEventStartAbout, NULL, NULL);
    submenu_add_item(
        app->submenu, "Config", ProtoPirateCustomEventStartConfig, NULL, NULL);
#ifdef ENABLE_TIMING_TUNER_SCENE
    submenu_add_item(
        app->submenu, "Timing Tuner", ProtoPirateCustomEventStartTimingTuner, NULL, NULL);
#endif

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSubmenu);
}

bool protopirate_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventStartReceive:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiver);
            return true;
        case ProtoPirateCustomEventStartSaved:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSaved);
            return true;
        case ProtoPirateCustomEventStartSubDecode:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSubDecode);
            return true;
        case ProtoPirateCustomEventStartAbout:
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "ProtoPirateTX v2.0");
            widget_add_string_element(
                app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary,
                "Decode & TX rolling-code");
            widget_add_string_element(
                app->widget, 64, 37, AlignCenter, AlignTop, FontSecondary,
                "remote protocols");
            widget_add_string_element(
                app->widget, 64, 52, AlignCenter, AlignTop, FontSecondary,
                "Supports 11+ protocols");
            widget_add_string_element(
                app->widget, 64, 64, AlignCenter, AlignTop, FontSecondary,
                "with counter rollback");
            widget_add_string_element(
                app->widget, 64, 76, AlignCenter, AlignTop, FontSecondary,
                "by RocketGod");
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdWidget);
            return true;
        case ProtoPirateCustomEventStartConfig:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            return true;
#ifdef ENABLE_TIMING_TUNER_SCENE
        case ProtoPirateCustomEventStartTimingTuner:
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneTimingTuner);
            return true;
#endif
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        // Exit app
        return false;
    }

    return false;
}

void protopirate_scene_start_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
