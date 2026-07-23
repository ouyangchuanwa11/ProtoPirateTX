#include "protopirate_rb.h"

// ===================== 按钮菜单回调 =====================
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(type != InputTypePress) return;

    if(index == 0 || index == 1) {
        transmit_packet(app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency, 3);
    } else if(index == 2) {
        transmit_packet(app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency, 10);
    } else if(index == 3) {
        app->scene = SceneMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    }
}

static uint32_t navigation_callback(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    // If already on main menu, return VIEW_NONE to let the dispatcher exit/stop
    if(app->scene == SceneMainMenu) {
        return VIEW_NONE;
    }
    // Otherwise go back to main menu
    app->scene = SceneMainMenu;
    return ViewMenu;
}

// ===================== Custom Event Callback =====================
static bool custom_event_callback(void* context, uint32_t event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;

    switch(event) {
    case SceneMainMenu:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case SceneReceive:
        scene_receive_enter(app);
        return true; // scene_receive_enter triggers SceneResult when done
    case SceneResult:
        scene_result_enter(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewButtonMenu);
        return true;
    case SceneRollback:
        scene_rollback_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    case SceneReplay:
        scene_replay_enter(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    case SceneFreqSelect:
        scene_freq_select_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
        return true;
    case SceneInfo:
        scene_info_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    default:
        return false;
    }
}

// ===================== Nav Event Callback =====================
static bool nav_event_callback(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    // Global fallback: if any view doesn't have its own previous callback,
    // check current scene and navigate back to menu
    if(app->scene != SceneMainMenu) {
        app->scene = SceneMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    }
    return false;
}

// ===================== 应用生命周期 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
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

    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));

    app->rollback.base_counter = 0;
    app->rollback.target_counter = 0;
    app->rollback.step_size = 1;
    app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
    app->rollback.running = false;
    app->rollback.serial = 0;
    app->rollback.button = 1;
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
    view_dispatcher_remove_view(app->view_dispatcher, ViewLoading);
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

// ===================== 场景: 主菜单 =====================
static void scene_main_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->submenu_index = index;

    uint32_t event;
    switch(index) {
    case 0: event = SceneReceive; break;
    case 1: event = SceneReplay; break;
    case 2: event = SceneRollback; break;
    case 3: event = SceneFreqSelect; break;
    case 4: event = SceneInfo; break;
    case 5: view_dispatcher_stop(app->view_dispatcher); return;
    default: return;
    }

    app->scene = event;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void scene_main_menu_alloc(ProtoPirateApp* app) {
    submenu_free(app->submenu);
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "ProtoPirate RB");

    submenu_add_item(app->submenu, "   Receive Signal", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Replay Signal", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   RollBack Attack", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Set Frequency", 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Info", 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Exit", 5, scene_main_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), navigation_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewMenu, submenu_get_view(app->submenu));
}

// ===================== 场景: 接收 =====================
void scene_receive_enter(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Receiving...");
    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary,
                              "Listening...");

    // Show the receiving view first
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    // 模拟接收: 3秒超时
    uint32_t timeout = furi_get_tick() + 3000;
    while(furi_get_tick() < timeout) {
        furi_delay_ms(50);
    }

    // 模拟解码结果
    app->last_result.bits = 64;
    app->last_result.serial = 0x1234567;
    app->last_result.button = 2;
    strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
    strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
    app->last_result.counter = 0xABCD;
    app->last_result.crc_ok = true;
    app->last_result.data_hi = 0;
    app->last_result.data_lo = 0x1234567A;
    app->last_result.data_hi = 0xBC; // 40-bit value split across hi/lo

    app->scene = SceneResult;
    view_dispatcher_send_custom_event(app->view_dispatcher, SceneResult);
}

// ===================== 场景: 结果 =====================
void scene_result_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "DECODED!");

    char line[64];
    snprintf(line, sizeof(line), "%s %u-bit @ %lu kHz",
             app->last_result.proto, app->last_result.bits,
             app->frequency / 1000);
    widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Sn:0x%07lX %s",
             app->last_result.serial, app->last_result.btn_name);
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

    snprintf(line, sizeof(line), "Cnt:0x%04X CRC:%s",
             app->last_result.counter,
             app->last_result.crc_ok ? "OK" : "FAIL");
    widget_add_string_element(app->widget, 64, 54, AlignCenter, AlignTop, FontKeyboard, line);
}

void scene_result_enter(ProtoPirateApp* app) {
    scene_result_alloc(app);
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "Actions");
    button_menu_add_item(app->button_menu, "Transmit", 0, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Replay x3", 1, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Brute x10", 2, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Back", 3, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_set_selected_item(app->button_menu, 0);
    
    app->scene = SceneResult;
    
    // Set back navigation on the button menu view
    view_set_previous_callback(button_menu_get_view(app->button_menu), navigation_callback);
}

// ===================== 场景: RollBack攻击 =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "RollBack Attack");

    char line[64];
    snprintf(line, sizeof(line), "Serial: 0x%07lX", app->rollback.serial);
    widget_add_string_element(app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Cnt: 0x%04X -> 0x%04X  Step: %u",
             app->rollback.base_counter, app->rollback.target_counter,
             app->rollback.step_size);
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

    widget_add_string_element(app->widget, 64, 58, AlignCenter, AlignTop, FontSecondary,
                              "OK = Run Attack");
    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontKeyboard,
                              "BACK = Menu");

    // Set back navigation on the widget view
    View* widget_view = widget_get_view(app->widget);
    view_set_previous_callback(widget_view, navigation_callback);
    
    // Also store current scene for nav callback
    app->scene = SceneRollback;
}

// ===================== 场景: 重放 =====================
void scene_replay_enter(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Replay Signal");

    app->scene = SceneReplay;

    if(app->last_result.bits > 0) {
        char line[64];
        snprintf(line, sizeof(line), "%s  Sn:0x%07lX",
                 app->last_result.proto, app->last_result.serial);
        widget_add_string_element(app->widget, 64, 30, AlignCenter, AlignTop, FontKeyboard, line);
        widget_add_string_element(app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary,
                                  "Replaying...");
        widget_add_string_element(app->widget, 64, 68, AlignCenter, AlignTop, FontSecondary,
                                  "Hold BACK to exit");

        // Start TX in background, don't block ViewDispatcher
        transmit_packet(app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency, 1);
    } else {
        // Demo 模式: 发送测试帧
        widget_add_string_element(app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary,
                                  "Demo: sending test...");

        uint32_t demo_data_hi, demo_data_lo;
        rollback_build_frame(0x1234567, 2, 0x100, &demo_data_hi, &demo_data_lo);
        transmit_packet(app, demo_data_hi, demo_data_lo, app->frequency, 3);

        widget_add_string_element(app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary,
                                  "Demo TX sent! (x3)");
    }
    
    // Ensure back navigation works
    View* widget_view = widget_get_view(app->widget);
    view_set_previous_callback(widget_view, navigation_callback);
}

// ===================== 场景: 频率选择 =====================
void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_enter_callback(app->var_item_list, NULL, app);
    UNUSED(app);
    variable_item_list_add(app->var_item_list, "315.00 MHz (US)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "433.92 MHz (EU/Asia)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "868.35 MHz (EU)", 0, NULL, NULL);
    
    app->scene = SceneFreqSelect;
    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), navigation_callback);
}

// ===================== 场景: 信息 =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate RB");
    widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
                              "RollBack Edition v2.0");
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard,
                              "TX: REAL CC1101");
    widget_add_string_element(app->widget, 64, 52, AlignCenter, AlignTop, FontKeyboard,
                              "RollBack Attack Engine");
    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontSecondary,
                              "Momentum FW Dev");
    
    app->scene = SceneInfo;
    View* widget_view = widget_get_view(app->widget);
    view_set_previous_callback(widget_view, navigation_callback);
}

// ===================== 主入口 =====================
__attribute__((visibility("default"))) int32_t app_main(void* p) {
    UNUSED(p);
    ProtoPirateApp* app = protoPirateApp_alloc();

    // Register all views
    scene_main_menu_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ViewVarList, variable_item_list_get_view(app->var_item_list));
    view_dispatcher_add_view(app->view_dispatcher, ViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, ViewButtonMenu, button_menu_get_view(app->button_menu));
    view_dispatcher_add_view(app->view_dispatcher, ViewPopup, popup_get_view(app->popup));

    // Start with main menu
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    protoPirateApp_free(app);
    return 0;
}
