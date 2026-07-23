#include "protopirate_rb.h"

// ===================== 按钮回调 (SubmenuItemCallback: void(context, uint32_t)) =====================
// button_menu_add_item still uses old (void*, int32_t, InputType) callback
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app || type != InputTypePress) return;

    switch(index) {
    case 0: // Send x3
        transmit_packet(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 3);
        break;
    case 1: // Send x5
        transmit_packet_nonblock(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 5);
        break;
    case 2: // Send x10
        transmit_packet_nonblock(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 10);
        break;
    case 3: // Back to menu
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

static void rollback_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;

    switch(index) {
    case 0: // Start RollBack Attack
        app->rollback.running = true;
        app->rollback.current_counter = app->rollback.base_counter;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackToggle);
        break;
    case 1: // Return to menu
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

static void replay_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;

    switch(index) {
    case 0: // Replay Car Signal
        if(app->last_result.bits > 0) {
            transmit_packet_nonblock(app, app->last_result.data_hi,
                app->last_result.data_lo, app->frequency, 5);
        } else {
            // No captured signal, use demo
            uint32_t hi, lo;
            rollback_build_frame(0x1234567, 2, 0x100, &hi, &lo);
            transmit_packet_nonblock(app, hi, lo, app->frequency, 5);
        }
        break;
    case 1: // Return to menu
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

static void receive_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;

    switch(index) {
    case 0: // Capture Signal
        // Demo mode: simulate a capture
        app->last_result.bits = 64;
        app->last_result.serial = 0x1234567;
        app->last_result.button = 2;
        strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
        strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
        app->last_result.counter = 0xABCD;
        app->last_result.crc_ok = true;
        rollback_build_frame(0x1234567, 2, 0xABCD,
            &app->last_result.data_hi, &app->last_result.data_lo);

        // Save to rollback params
        app->rollback.serial = app->last_result.serial;
        app->rollback.button = app->last_result.button;
        app->rollback.base_counter = app->last_result.counter;
        strncpy(app->rollback.proto, app->last_result.proto, sizeof(app->rollback.proto));
        view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
        break;
    case 1: // Return to menu
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

// ===================== 自定义事件回调 =====================
static bool custom_event_callback(void* context, uint32_t event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return false;

    switch(event) {
    case EventGoMenu:
        scene_main_menu_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventReceive:
        scene_receive_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventRollback:
        scene_rollback_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventReplay:
        scene_replay_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventFreqSelect:
        scene_freq_select_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
        return true;
    case EventInfo:
        scene_info_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    case EventReceiveDone:
        scene_result_main_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewButtonMenu);
        return true;
    case EventRollbackToggle:
        scene_rollback_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    default:
        return false;
    }
}

// ===================== 导航回调 =====================
static uint32_t back_to_menu_callback(void* context) {
    UNUSED(context);
    // Return to main menu view
    return ViewMenu;
}

// ===================== Nav Event Callback (global) =====================
static bool nav_event_callback(void* context) {
    UNUSED(context);
    return false;
}

// ===================== 应用生命周期 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    if(!app) return NULL;

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, nav_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    app->var_item_list = variable_item_list_alloc();
    app->text_box = text_box_alloc();
    app->widget = widget_alloc();
    app->button_menu = button_menu_alloc();
    app->popup = popup_alloc();

    app->frequency = DEFAULT_FREQ;
    app->scene = SceneMainMenu;
    app->submenu_index = 0;
    app->result_menu_index = 0;
    app->history_count = 0;
    app->last_raw = furi_string_alloc();
    app->counter = 0;

    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));

    app->rollback.base_counter = 0;
    app->rollback.target_counter = 100;
    app->rollback.step_size = 1;
    app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
    app->rollback.running = false;
    app->rollback.current_counter = 0;
    app->rollback.serial = 0x1234567;
    app->rollback.button = 2;
    app->rollback.counter_limit = ROLLBACK_LIMIT;
    strncpy(app->rollback.proto, "Kia V0", sizeof(app->rollback.proto));

    return app;
}

void protoPirateApp_free(ProtoPirateApp* app) {
    furi_assert(app);
    furi_string_free(app->last_raw);
    for(uint8_t i = 0; i < app->history_count; i++) {
        if(app->history[i].raw_data) furi_string_free(app->history[i].raw_data);
    }
    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, ViewButtonMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewPopup);
    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    text_box_free(app->text_box);
    widget_free(app->widget);
    button_menu_free(app->button_menu);
    popup_free(app->popup);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

// ===================== 主菜单回调 =====================
static void scene_main_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    app->submenu_index = index;

    uint32_t event;
    switch(index) {
    case 0: event = EventReceive; break;
    case 1: event = EventReplay; break;
    case 2: event = EventRollback; break;
    case 3: event = EventFreqSelect; break;
    case 4: event = EventInfo; break;
    case 5:
        // Exit
        view_dispatcher_stop(app->view_dispatcher);
        return;
    default: return;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void scene_main_menu_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ProtoPirate TX");

    submenu_add_item(app->submenu, "  Capture Signal", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  Replay Signal", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  RollBack Attack", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  Set Frequency", 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  About", 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  EXIT", 5, scene_main_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== Capture Signal（菜单页） =====================
void scene_receive_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Capture Signal");

    char freq_str[40];
    snprintf(freq_str, sizeof(freq_str), "  Capture @ %lu.%lu MHz",
             app->frequency / 1000000,
             (app->frequency % 1000000) / 1000);
    submenu_add_item(app->submenu, freq_str, 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  >> Capture Now <<", 0, receive_menu_callback, app);
    submenu_add_item(app->submenu, "  Back to Menu", 1, receive_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== 结果页（ButtonMenu） =====================
void scene_result_main_alloc(ProtoPirateApp* app) {
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "Signal Captured!");

    char sn_str[20];
    snprintf(sn_str, sizeof(sn_str), "Send x3 (Sn:0x%lX)", app->last_result.serial);
    button_menu_add_item(app->button_menu, sn_str, 0, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Send x5", 1, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Send x10", 2, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Back to Menu", 3, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_set_selected_item(app->button_menu, 0);

    view_set_previous_callback(button_menu_get_view(app->button_menu), back_to_menu_callback);
}

// ===================== RollBack Attack（菜单页） =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "RollBack Attack");

    char line[40];
    snprintf(line, sizeof(line), "  Sn:0x%07lX Btn:%u", app->rollback.serial, app->rollback.button);
    submenu_add_item(app->submenu, line, 0, scene_main_menu_callback, app);

    snprintf(line, sizeof(line), "  Cnt:%u->%u Step:%u",
             app->rollback.base_counter, app->rollback.target_counter, app->rollback.step_size);
    submenu_add_item(app->submenu, line, 0, scene_main_menu_callback, app);

    if(app->rollback.running) {
        snprintf(line, sizeof(line), "  >> ATTACK: %u/%u <<",
                 app->rollback.current_counter, app->rollback.target_counter);
    } else {
        strncpy(line, "  >> START ATTACK <<", sizeof(line));
    }
    submenu_add_item(app->submenu, line, 0, rollback_menu_callback, app);

    submenu_add_item(app->submenu, "  Back to Menu", 1, rollback_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);

    // If running, do a TX burst
    if(app->rollback.running) {
        int32_t diff = (int32_t)app->rollback.target_counter - app->rollback.current_counter;
        if(diff > 0) {
            uint32_t dhi, dlo;
            rollback_build_frame(app->rollback.serial, app->rollback.button,
                               (uint32_t)app->rollback.current_counter, &dhi, &dlo);
            transmit_packet_nonblock(app, dhi, dlo, app->frequency, 3);

            app->rollback.current_counter += app->rollback.step_size;

            if(app->rollback.current_counter >= app->rollback.target_counter ||
               app->rollback.current_counter >= app->rollback.counter_limit) {
                app->rollback.running = false;
            }
        } else {
            app->rollback.running = false;
        }
    }
}

// ===================== Replay Signal（菜单页） =====================
void scene_replay_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Replay Signal");

    if(app->last_result.bits > 0) {
        char line[40];
        snprintf(line, sizeof(line), "  %s Sn:0x%lX",
                 app->last_result.proto, app->last_result.serial);
        submenu_add_item(app->submenu, line, 0, scene_main_menu_callback, app);

        submenu_add_item(app->submenu, "  >> Replay Now <<", 0, replay_menu_callback, app);
    } else {
        submenu_add_item(app->submenu, "  No captured signal", 0, scene_main_menu_callback, app);
        submenu_add_item(app->submenu, "  >> Send Demo Frame <<", 0, replay_menu_callback, app);
    }

    submenu_add_item(app->submenu, "  Back to Menu", 1, replay_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== 频率选择 =====================
static const uint32_t FREQ_TABLE[] = {315000000, 433920000, 868350000};
static const char* FREQ_NAMES[] = {"315.00 MHz (US)", "433.92 MHz (EU/Asia)", "868.35 MHz (EU)"};

static void freq_change_callback(VariableItem* item) {
    if(!item) return;
    ProtoPirateApp* app = variable_item_get_context(item);
    if(!app) return;

    uint8_t index = variable_item_get_current_value_index(item);
    if(index < 3) {
        app->frequency = FREQ_TABLE[index];
        variable_item_set_current_value_text(item, FREQ_NAMES[index]);
    }
}

void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);

    uint8_t default_idx = 1;
    if(app->frequency == 315000000) default_idx = 0;
    else if(app->frequency == 433920000) default_idx = 1;
    else if(app->frequency == 868350000) default_idx = 2;

    for(int i = 0; i < 3; i++) {
        VariableItem* item = variable_item_list_add(
            app->var_item_list, FREQ_NAMES[i], 0,
            freq_change_callback, app);
        if(i == default_idx) {
            variable_item_set_current_value_index(item, i);
            variable_item_set_current_value_text(item, FREQ_NAMES[i]);
        }
    }

    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), back_to_menu_callback);
}

// ===================== About（信息页 - widget只读） =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate TX");
    widget_add_string_element(app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary,
                              "RollBack Edition v2.0");
    widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignTop, FontKeyboard,
                              "TX: Real CC1101");
    widget_add_string_element(app->widget, 64, 44, AlignCenter, AlignTop, FontKeyboard,
                              "RollBack Attack Engine");
    widget_add_string_element(app->widget, 64, 56, AlignCenter, AlignTop, FontSecondary,
                              "Momentum Firmware");
    widget_add_string_element(app->widget, 64, 68, AlignCenter, AlignTop, FontPrimary,
                              "BACK = Menu");

    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== 主入口 =====================
__attribute__((visibility("default"))) int32_t app_main(void* p) {
    UNUSED(p);

    ProtoPirateApp* app = protoPirateApp_alloc();
    if(!app) return -1;

    view_dispatcher_add_view(app->view_dispatcher, ViewMenu,
                            submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget,
                            widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ViewVarList,
                            variable_item_list_get_view(app->var_item_list));
    view_dispatcher_add_view(app->view_dispatcher, ViewTextBox,
                            text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, ViewButtonMenu,
                            button_menu_get_view(app->button_menu));
    view_dispatcher_add_view(app->view_dispatcher, ViewPopup,
                            popup_get_view(app->popup));

    scene_main_menu_alloc(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);

    view_dispatcher_run(app->view_dispatcher);

    transmit_packet_stop(app);

    protoPirateApp_free(app);
    return 0;
}
