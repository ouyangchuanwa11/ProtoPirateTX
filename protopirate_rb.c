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
    }
}

static uint32_t navigation_callback(void* context) {
    UNUSED(context);
    return 0;
}

// ===================== 应用生命周期 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

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
    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    text_box_free(app->text_box);
    widget_free(app->widget);
    button_menu_free(app->button_menu);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

// ===================== 场景: 主菜单 =====================
static void scene_main_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->submenu_index = index;
    switch(index) {
    case 0: app->scene = SceneReceive; break;
    case 1: app->scene = SceneReplay; break;
    case 2: app->scene = SceneRollback; break;
    case 3: app->scene = SceneFreqSelect; break;
    case 4: app->scene = SceneInfo; break;
    case 5: view_dispatcher_stop(app->view_dispatcher); break;
    }
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
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));
}

// ===================== 场景: 接收 =====================
void scene_receive_alloc(ProtoPirateApp* app) {
    UNUSED(app);
}

void scene_receive_enter(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Receiving...");
    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary,
                              "Waiting...");

    // 模拟接收: 5秒超时
    uint32_t timeout = furi_get_tick() + 5000;
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
    app->last_result.data_lo = 0x1234567;
    app->scene = SceneResult;
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
}

// ===================== 场景: RollBack攻击 =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "RollBack Attack");

    char line[64];
    snprintf(line, sizeof(line), "Serial: 0x%07lX", app->rollback.serial);
    widget_add_string_element(app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Base Cnt: 0x%04X  Step: %u",
             app->rollback.base_counter, app->rollback.step_size);
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

    widget_add_string_element(app->widget, 64, 58, AlignCenter, AlignTop, FontSecondary,
                              "OK = Run Attack");
    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontKeyboard,
                              "BACK = Menu");
}

// ===================== 场景: 重放 =====================
void scene_replay_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Replay Signal");

    if(app->last_result.bits > 0) {
        char line[64];
        snprintf(line, sizeof(line), "%s  Sn:0x%07lX",
                 app->last_result.proto, app->last_result.serial);
        widget_add_string_element(app->widget, 64, 30, AlignCenter, AlignTop, FontKeyboard, line);
        widget_add_string_element(app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary,
                                  "OK = Replay");
    } else {
        widget_add_string_element(app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary,
                                  "No signal captured");
    }
}

// ===================== 场景: 频率选择 =====================
void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_enter_callback(app->var_item_list, NULL, app);
    UNUSED(app);
    variable_item_list_add(app->var_item_list, "315.00 MHz (US)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "433.92 MHz (EU/Asia)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "868.35 MHz (EU)", 0, NULL, NULL);
}

// ===================== 场景: 信息 =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate RB");
    widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
                              "RollBack Edition v2.0");
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard,
                              "TX: FULLY UNLOCKED");
    widget_add_string_element(app->widget, 64, 52, AlignCenter, AlignTop, FontKeyboard,
                              "RollBack Attack Engine");
}

// ===================== 主入口 =====================
int32_t app_main(void* p) __attribute__((visibility("default"))) {
    UNUSED(p);
    ProtoPirateApp* app = protoPirateApp_alloc();

    scene_main_menu_alloc(app);
    scene_receive_alloc(app);
    scene_result_alloc(app);
    scene_rollback_alloc(app);
    scene_replay_alloc(app);
    scene_info_alloc(app);
    scene_freq_select_alloc(app);

    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    view_dispatcher_run(app->view_dispatcher);

    protoPirateApp_free(app);
    return 0;
}
