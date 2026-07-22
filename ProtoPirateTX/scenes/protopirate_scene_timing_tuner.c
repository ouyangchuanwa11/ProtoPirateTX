#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/modules/variable_item_list.h>

static float timing_factor = 1.0f;

static void timing_increase_callback(VariableItem* item) {
    timing_factor += 0.1f;
    if(timing_factor > 3.0f) timing_factor = 3.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fx", (double)timing_factor);
    variable_item_set_current_value_text(item, buf);
}

static void timing_decrease_callback(VariableItem* item) {
    timing_factor -= 0.1f;
    if(timing_factor < 0.3f) timing_factor = 0.3f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fx", (double)timing_factor);
    variable_item_set_current_value_text(item, buf);
}

static void timing_reset_callback(VariableItem* item) {
    timing_factor = 1.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fx", (double)timing_factor);
    variable_item_set_current_value_text(item, buf);
}

void protopirate_scene_timing_tuner_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    variable_item_list_reset(app->variable_item_list);

    // Load current timing factor
    timing_factor = app->timing_factor;

    // Timing factor display
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Timing Factor", 1, NULL, NULL);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fx", (double)timing_factor);
    variable_item_set_current_value_text(item, buf);

    variable_item_list_add(app->variable_item_list, "---", 1, NULL, NULL);

    variable_item_list_add(
        app->variable_item_list, "[+] Increase (OK)", 1, timing_increase_callback, NULL);

    variable_item_list_add(
        app->variable_item_list, "[-] Decrease (OK)", 1, timing_decrease_callback, NULL);

    variable_item_list_add(
        app->variable_item_list, "[R] Reset (OK)", 1, timing_reset_callback, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdVariableItemList);
}

bool protopirate_scene_timing_tuner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        // Update the app's timing factor
        app->timing_factor = timing_factor;
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        // Save the timing factor
        app->timing_factor = timing_factor;
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void protopirate_scene_timing_tuner_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->timing_factor = timing_factor;
    variable_item_list_reset(app->variable_item_list);
}
