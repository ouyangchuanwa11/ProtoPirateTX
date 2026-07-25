#include "protopirate_rb.h"

// ===================== Scene Handlers =====================
static void scene_manager_init(ProtoPirateApp* app) {
    app->scene_manager = scene_manager_alloc(&protopirate_scene_handlers, app);
}

// ===================== View Dispatcher =====================
static void view_dispatcher_init(ProtoPirateApp* app) {
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, GuiLayerWindow);
}

// ===================== Main Menu Scene =====================
void protopirate_scene_main_menu_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneMainMenu;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ProtoPirate RB v3.1");
    submenu_add_item(app->submenu, "Read / Decode", 0, protopirate_scene_main_menu_on_event, app);
    submenu_add_item(app->submenu, "RollBack Attack", 1, protopirate_scene_main_menu_on_event, app);
    submenu_add_item(app->submenu, "Batch Send", 2, protopirate_scene_main_menu_on_event, app);
    submenu_add_item(app->submenu, "TX Custom Frame", 3, protopirate_scene_main_menu_on_event, app);
    submenu_add_item(app->submenu, "TX Raw Signal", 4, protopirate_scene_main_menu_on_event, app);
    submenu_add_item(app->submenu, "About", 5, protopirate_scene_main_menu_on_event, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewSubmenu);
}

bool protopirate_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case 0:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneDecode);
                break;
            case 1:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneRollbackConfig);
                break;
            case 2:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneBatchConfig);
                break;
            case 3:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneTXCustom);
                break;
            case 4:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneTXRaw);
                break;
            case 5:
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneAbout);
                break;
        }
    }
    return false;
}

void protopirate_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Decode Scene =====================
void protopirate_scene_decode_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneDecode;
    
    widget_reset(app->widget);
    
    // Simulated capture - in real use, Sub-GHz RAW capture would feed data here
    // For now we show instructions
    furi_string_printf(app->info_str,
        "Decode Mode\n\n"
        "Use Flipper Sub-GHz app to capture\n"
        "a signal first (Read RAW).\n\n"
        "Then come here with the RAW data.\n\n"
        "Press OK to simulate decode\n"
        "of sample data.");
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "Simulate", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_decode_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        // Simulate a captured Kia V0 signal
        FuriString* test_signal = furi_string_alloc();
        furi_string_set_str(test_signal, 
            "8000 -4000 560 -280 280 -560 560 -280 560 -280 280 -560 "
            "560 -280 280 -560 280 -560 560 -280 280 -560 560 -280 "
            "560 -280 560 -280 280 -560 560 -280 280 -560 280 -560 "
            "560 -280 280 -560 560 -280 280 -560 280 -560 560 -280 "
            "560 -280 280 -560 280 -560 560 -280 560 -280 280 -560 "
            "560 -280 280 -560 560 -280 560 -280 560 -280 280 -560 "
            "280 -560 560 -280 280 -560 280 -560 560 -280 560 -280 "
            "560 -280 280 -560 560 -280 280 -560 560 -280 280 -560 "
            "280 -560 560 -280 560 -280 280 -560 560 -280 560 -280 "
            "12000");
        
        DecodeResult* result = decode_signal(app, test_signal);
        if(result) {
            memcpy(&app->last_result, result, sizeof(DecodeResult));
            // Also populate rollback state from decode result
            strncpy(app->rollback.proto, result->proto, sizeof(app->rollback.proto));
            app->rollback.serial = result->serial;
            app->rollback.button = result->button;
            app->rollback.base_counter = result->counter;
            app->rollback.target_counter = result->counter + 50;
            app->rollback.protocol_type = Proto_Kia_V0;
            
            free(result);
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneDecodeResult);
        } else {
            furi_string_printf(app->info_str, "Decode FAILED\n\nCould not identify protocol.\nTry different signal.");
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
            widget_add_button(app->widget, GuiButtonTypeCenter, "Back", NULL, app);
        }
        furi_string_free(test_signal);
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void protopirate_scene_decode_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Decode Result Scene =====================
void protopirate_scene_decode_result_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneDecodeResult;
    
    widget_reset(app->widget);
    
    DecodeResult* r = &app->last_result;
    furi_string_printf(app->info_str,
        "DECODED SIGNAL\n"
        "Proto: %s\n"
        "Serial: 0x%08lX\n"
        "Button: %s (%u)\n"
        "Counter: %u (0x%04X)\n"
        "Bits: %u\n"
        "Encrypted: %s\n\n"
        "OK=Send Once  BACK=Menu",
        r->proto, r->serial, r->btn_name, r->button,
        r->counter, r->counter, r->bits,
        r->encrypted ? "Yes" : "No");
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "Send Once", NULL, app);
    widget_add_button(app->widget, GuiButtonTypeLeft, "Back", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_decode_result_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        // Send once
        if(!g_device) tx_device_init();
        if(g_device) {
            rollback_send_single(app, app->last_result.serial, app->last_result.button, app->last_result.counter);
            notification_message(app->notifications, &sequence_success);
        } else {
            notification_message(app->notifications, &sequence_error);
        }
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, ProtoPirateSceneMainMenu);
    }
    return false;
}

void protopirate_scene_decode_result_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Rollback Config Scene =====================
static void rollback_proto_callback(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->rollback.protocol_type = idx;
    switch(idx) {
        case 0: strcpy(app->rollback.proto, "Kia V0"); app->frequency = 433920000; break;
        case 1: strcpy(app->rollback.proto, "Kia V1"); app->frequency = 433920000; break;
        case 2: strcpy(app->rollback.proto, "Kia V2"); app->frequency = 433920000; break;
        case 3: strcpy(app->rollback.proto, "Ford"); app->frequency = 315000000; break;
        case 4: strcpy(app->rollback.proto, "Subaru"); app->frequency = 433920000; break;
        case 5: strcpy(app->rollback.proto, "Fiat"); app->frequency = 433920000; break;
        case 6: strcpy(app->rollback.proto, "Chrysler"); app->frequency = 315000000; break;
        case 7: strcpy(app->rollback.proto, "Honda"); app->frequency = 315000000; break;
        case 8: strcpy(app->rollback.proto, "Toyota"); app->frequency = 315000000; break;
        case 9: strcpy(app->rollback.proto, "StarLine"); app->frequency = 433920000; break;
    }
    variable_item_set_current_value_text(item, app->rollback.proto);
}

static void rollback_button_callback(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->rollback.button = rollback_get_button_value(app->rollback.protocol_type, idx);
    char txt[16];
    snprintf(txt, sizeof(txt), "%s (%u)", get_button_name(app->rollback.proto, app->rollback.button), app->rollback.button);
    variable_item_set_current_value_text(item, txt);
}

static void rollback_mode_callback(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->rollback.bidirectional = (idx == 1);
    variable_item_set_current_value_text(item, idx == 0 ? "Forward" : "Bidirectional");
}

void protopirate_scene_rollback_config_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneRollbackConfig;
    
    variable_item_list_reset(app->variable_item_list);
    variable_item_list_set_header(app->variable_item_list, "RollBack Config");
    
    // Protocol
    VariableItem* item = variable_item_list_add(app->variable_item_list, "Protocol", 10, rollback_proto_callback, app);
    variable_item_set_current_value_index(item, app->rollback.protocol_type);
    variable_item_set_current_value_text(item, app->rollback.proto);
    
    // Button
    item = variable_item_list_add(app->variable_item_list, "Button", 4, rollback_button_callback, app);
    variable_item_set_current_value_index(item, app->rollback.button > 4 ? 3 : app->rollback.button - 1);
    char txt[16];
    snprintf(txt, sizeof(txt), "%s (%u)", get_button_name(app->rollback.proto, app->rollback.button), app->rollback.button);
    variable_item_set_current_value_text(item, txt);
    
    // Mode
    item = variable_item_list_add(app->variable_item_list, "Mode", 2, rollback_mode_callback, app);
    variable_item_set_current_value_index(item, app->rollback.bidirectional ? 1 : 0);
    variable_item_set_current_value_text(item, app->rollback.bidirectional ? "Bidirectional" : "Forward");
    
    // Add Run button
    variable_item_list_add(app->variable_item_list, "START ATTACK", 1, NULL, NULL);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
}

bool protopirate_scene_rollback_config_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        // Start attack
        if(!g_device) tx_device_init();
        if(!g_device) {
            notification_message(app->notifications, &sequence_error);
            return false;
        }
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneRollbackRun);
    }
    return false;
}

void protopirate_scene_rollback_config_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Rollback Run Scene =====================
void protopirate_scene_rollback_run_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneRollbackRun;
    
    widget_reset(app->widget);
    
    furi_string_printf(app->info_str,
        "RollBack ATTACK\n"
        "Proto: %s\n"
        "Serial: 0x%08lX\n"
        "Button: %s\n"
        "Counter: %u -> %u\n"
        "Mode: %s\n\n"
        "OK=Start  BACK=Cancel",
        app->rollback.proto,
        app->rollback.serial,
        get_button_name(app->rollback.proto, app->rollback.button),
        app->rollback.base_counter,
        app->rollback.target_counter,
        app->rollback.bidirectional ? "Bidirectional" : "Forward");
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "START", NULL, app);
    widget_add_button(app->widget, GuiButtonTypeLeft, "Cancel", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_rollback_run_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        // Start the attack
        notification_message(app->notifications, &sequence_single_vibro);
        
        widget_reset(app->widget);
        furi_string_printf(app->info_str, "RUNNING...\n\nPress BACK to stop.");
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
        widget_add_button(app->widget, GuiButtonTypeLeft, "STOP", NULL, app);
        
        if(app->rollback.bidirectional) {
            rollback_bidirectional_attack(app);
        } else {
            rollback_attack_run(app);
        }
        
        // Show results
        widget_reset(app->widget);
        furi_string_printf(app->info_str,
            "DONE!\n\nFrames sent: %lu\nLast counter: 0x%04X\n\nPress BACK to menu.",
            app->rollback.total_sent,
            app->rollback.current_counter);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
        widget_add_button(app->widget, GuiButtonTypeLeft, "Menu", NULL, app);
        
        notification_message(app->notifications, &sequence_success);
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        // Stop
        app->rollback.running = false;
        notification_message(app->notifications, &sequence_error);
    }
    return false;
}

void protopirate_scene_rollback_run_on_exit(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->rollback.running = false;
}

// ===================== Batch Config Scene =====================
void protopirate_scene_batch_config_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneBatchConfig;
    
    widget_reset(app->widget);
    app->batch.count = 100;
    
    furi_string_printf(app->info_str,
        "Batch Send\n\n"
        "Will send %lu frames rapidly.\n"
        "Uses last decode result.\n\n"
        "OK=Start  BACK=Menu",
        app->batch.count);
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "START", NULL, app);
    widget_add_button(app->widget, GuiButtonTypeLeft, "Back", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_batch_config_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(!g_device) tx_device_init();
        if(!g_device) {
            notification_message(app->notifications, &sequence_error);
            return false;
        }
        
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneBatchRun);
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void protopirate_scene_batch_config_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Batch Run Scene =====================
void protopirate_scene_batch_run_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneBatchRun;
    
    widget_reset(app->widget);
    furi_string_printf(app->info_str, "Batch Running...\n\n%lu / %lu", 
                       app->batch.sent_so_far, app->batch.count);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeLeft, "STOP", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
    
    batch_send_start(app);
    
    widget_reset(app->widget);
    furi_string_printf(app->info_str, "Batch DONE!\n\nSent: %lu frames\n\nBACK=Menu", app->batch.sent_so_far);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeLeft, "Menu", NULL, app);
}

bool protopirate_scene_batch_run_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom + 1) {
        batch_send_stop(app);
    }
    return false;
}

void protopirate_scene_batch_run_on_exit(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    batch_send_stop(app);
}

// ===================== TX Custom Frame Scene =====================
void protopirate_scene_tx_custom_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneTXCustom;
    
    widget_reset(app->widget);
    
    furi_string_printf(app->info_str,
        "TX Custom Frame\n\n"
        "Serial: 0x%08lX\n"
        "Button: %s\n"
        "Counter: %u\n"
        "Freq: %lu Hz\n\n"
        "OK=Send  BACK=Menu",
        app->rollback.serial,
        get_button_name(app->rollback.proto, app->rollback.button),
        app->rollback.base_counter,
        app->frequency);
    
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "SEND", NULL, app);
    widget_add_button(app->widget, GuiButtonTypeLeft, "Back", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_tx_custom_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(!g_device) tx_device_init();
        if(g_device) {
            rollback_send_single(app, app->rollback.serial, app->rollback.button, app->rollback.base_counter);
            notification_message(app->notifications, &sequence_success);
        } else {
            notification_message(app->notifications, &sequence_error);
        }
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void protopirate_scene_tx_custom_on_exit(void* context) {
    UNUSED(context);
}

// ===================== TX RAW Scene =====================
void protopirate_scene_tx_raw_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneTXRaw;
    
    widget_reset(app->widget);
    furi_string_printf(app->info_str,
        "TX RAW Signal\n\n"
        "Sends the captured RAW signal\n"
        "from the last read operation.\n\n"
        "OK=Send  BACK=Menu");
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeCenter, "SEND RAW", NULL, app);
    widget_add_button(app->widget, GuiButtonTypeLeft, "Back", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_tx_raw_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(!g_device) tx_device_init();
        if(g_device && app->captured_signal) {
            transmit_raw(app, app->captured_signal, app->frequency, 3);
            notification_message(app->notifications, &sequence_success);
        } else {
            notification_message(app->notifications, &sequence_error);
        }
    } else if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void protopirate_scene_tx_raw_on_exit(void* context) {
    UNUSED(context);
}

// ===================== About Scene =====================
void protopirate_scene_about_on_enter(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->current_scene = ProtoPirateSceneAbout;
    
    widget_reset(app->widget);
    furi_string_printf(app->info_str,
        "ProtoPirate RB v3.1\n"
        "RollBack Attack Tool\n\n"
        "Protocols:\n"
        "Kia/Hyundai, Ford, Subaru\n"
        "Fiat, Chrysler, Honda\n"
        "Toyota, StarLine, BFT\n\n"
        "TX ENABLED\n"
        "CC1101 Radio Active\n\n"
        "2025 ProtoPirate RB\n"
        "github.com/ouyangchuanwa11\n"
        "/ProtoPirateTX\n\n"
        "BACK to return");
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->info_str));
    widget_add_button(app->widget, GuiButtonTypeLeft, "Back", NULL, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_about_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(event.type == SceneManagerEventTypeCustom + 1) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void protopirate_scene_about_on_exit(void* context) {
    UNUSED(context);
}

// ===================== Custom Event Callback =====================
static bool protopirate_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

// ===================== Back Event Callback =====================
static bool protopirate_back_event_callback(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    
    // In rollback run scene, back = stop
    if(app->current_scene == ProtoPirateSceneRollbackRun && app->rollback.running) {
        scene_manager_handle_custom_event(app->scene_manager, 1);
        return true;
    }
    
    return scene_manager_handle_back_event(app->scene_manager);
}

// ===================== Scene Handler Table =====================
const SceneManagerHandlers protopirate_scene_handlers = {
    .on_enter_handlers = NULL,
    .on_event_handlers = NULL,
    .on_exit_handlers = NULL,
    .scene_num = 0,
};

// ===================== App Entry Point =====================
void protopirate_rb_app(void* p) {
    UNUSED(p);
    
    // Init CC1101 TX device
    tx_device_init();
    
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    memset(app, 0, sizeof(ProtoPirateApp));
    
    // Init notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Allocate views
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->variable_item_list = variable_item_list_alloc();
    
    // Allocate string
    app->info_str = furi_string_alloc();
    app->captured_signal = furi_string_alloc();
    
    // Set default values
    app->frequency = 433920000;
    app->tx_repeats = 5;
    app->tx_busy = false;
    
    strcpy(app->rollback.proto, "Kia V0");
    app->rollback.protocol_type = Proto_Kia_V0;
    app->rollback.serial = 0x12345678;
    app->rollback.button = 1; // Lock
    app->rollback.base_counter = 1000;
    app->rollback.target_counter = 1050;
    app->rollback.step_size = 1;
    app->rollback.burst_count = 3;
    app->rollback.running = false;
    app->rollback.bidirectional = false;
    
    app->batch.count = 100;
    app->batch.active = false;
    
    // Init scene manager manually (no fap entry helpers)
    app->scene_manager = scene_manager_alloc(NULL, app);
    
    view_dispatcher_init(app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, protopirate_custom_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, protopirate_back_event_callback);
    
    // Register views
    view_dispatcher_add_view(app->view_dispatcher, ProtoPirateViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, ProtoPirateViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ProtoPirateViewVariableItemList, variable_item_list_get_view(app->variable_item_list));
    
    // Start at main menu
    app->current_scene = ProtoPirateSceneMainMenu;
    protopirate_scene_main_menu_on_enter(app);
    
    view_dispatcher_run(app->view_dispatcher);
    
    // Cleanup
    transmit_packet_stop(app);
    tx_device_deinit();
    
    view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
    
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    
    submenu_free(app->submenu);
    widget_free(app->widget);
    variable_item_list_free(app->variable_item_list);
    
    furi_string_free(app->info_str);
    furi_string_free(app->captured_signal);
    
    furi_record_close(RECORD_NOTIFICATION);
    
    free(app);
}