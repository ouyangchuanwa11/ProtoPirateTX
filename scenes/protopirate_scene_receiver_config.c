#include "protopirate_scene.h"
#include "protopirate_scene_config.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/modules/variable_item_list.h>

static const uint32_t frequency_values[] = {
    300000000, 303875000, 304250000, 315000000, 318000000,
    345000000, 390000000, 418000000, 433075000, 433220000,
    433420000, 433920000, 434420000, 434775000, 434790000,
    438900000, 868350000, 868400000, 868700000, 868800000,
    868950000, 906400000, 915000000, 925000000,
};

static const char* const frequency_names[] = {
    "300.000 MHz", "303.875 MHz", "304.250 MHz", "315.000 MHz", "318.000 MHz",
    "345.000 MHz", "390.000 MHz", "418.000 MHz", "433.075 MHz", "433.220 MHz",
    "433.420 MHz", "433.920 MHz", "434.420 MHz", "434.775 MHz", "434.790 MHz",
    "438.900 MHz", "868.350 MHz", "868.400 MHz", "868.700 MHz", "868.800 MHz",
    "868.950 MHz", "906.400 MHz", "915.000 MHz", "925.000 MHz",
};

static uint32_t frequency_index = 6; // default to 433.920 MHz

static void frequency_change_callback(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    frequency_index = index;
    app->current_frequency = frequency_values[index];

    char buf[32];
    snprintf(buf, sizeof(buf), "%s", frequency_names[index]);
    variable_item_set_current_value_text(item, buf);

    // If receiver is active, restart with new frequency
    protopirate_rx_stop(app);
    protopirate_rx_start(app);
}

void protopirate_scene_receiver_config_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    variable_item_list_reset(app->variable_item_list);

    // Find current frequency index
    frequency_index = 0;
    for(uint32_t i = 0; i < COUNT_OF(frequency_values); i++) {
        if(frequency_values[i] == app->current_frequency) {
            frequency_index = i;
            break;
        }
    }

    // Frequency selector
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Frequency", COUNT_OF(frequency_values), frequency_change_callback, app);
    variable_item_set_current_value_index(item, frequency_index);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", frequency_names[frequency_index]);
    variable_item_set_current_value_text(item, buf);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdVariableItemList);
}

bool protopirate_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    UNUSED(context);
    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }
    return false;
}

void protopirate_scene_receiver_config_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    variable_item_list_reset(app->variable_item_list);
}
