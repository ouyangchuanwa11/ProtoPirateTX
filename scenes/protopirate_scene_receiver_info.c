#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/modules/widget.h>

void protopirate_scene_receiver_info_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    widget_reset(app->widget);

    if(app->last_result.valid) {
        char buf[64];
        widget_add_string_element(
            app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
            app->last_result.protocol->display_name);

        widget_add_string_element(
            app->widget, 0, 20, AlignLeft, AlignTop, FontSecondary,
            "--- Decode Info ---");

        snprintf(buf, sizeof(buf), "Serial: %08lX", app->last_result.serial);
        widget_add_string_element(app->widget, 0, 32, AlignLeft, AlignTop, FontSecondary, buf);

        snprintf(buf, sizeof(buf), "Button: %lu", app->last_result.button);
        widget_add_string_element(app->widget, 0, 42, AlignLeft, AlignTop, FontSecondary, buf);

        snprintf(buf, sizeof(buf), "Counter: %lu", app->last_result.counter);
        widget_add_string_element(app->widget, 0, 52, AlignLeft, AlignTop, FontSecondary, buf);

        snprintf(buf, sizeof(buf), "CRC: %04lX", app->last_result.crc);
        widget_add_string_element(app->widget, 0, 62, AlignLeft, AlignTop, FontSecondary, buf);

        if(app->last_result.protocol->features & PROTO_HAS_FIXED) {
            snprintf(buf, sizeof(buf), "Fixed: %08lX", app->last_result.fixed_code);
            widget_add_string_element(app->widget, 0, 72, AlignLeft, AlignTop, FontSecondary, buf);
        }
    } else {
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignCenter, FontPrimary,
            "No signal decoded");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdWidget);
}

bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    UNUSED(context);
    UNUSED(event);
    return false;
}

void protopirate_scene_receiver_info_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    widget_reset(app->widget);
}
