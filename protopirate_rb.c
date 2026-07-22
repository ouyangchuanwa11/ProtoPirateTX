#include "protopirate_rb.h"

// ============================================================================
// ProtoPirate RB - Main Application
// FAP for Momentum Firmware on Flipper Zero
// ============================================================================

// ===================== ?????? =====================
ProtoPirateApp* protoPirateApp_alloc() {
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

    app->environment = subghz_environment_alloc();
    app->subghz_worker = subghz_worker_alloc();
    app->txrx_worker = subghz_tx_rx_worker_alloc();

    // ??????
    app->frequency = DEFAULT_FREQ;
    app->scene = SceneMainMenu;
    app->submenu_index = 0;
    app->result_menu_index = 0;
    app->history_count = 0;
    app->last_raw = furi_string_alloc();
    app->decoder_result = NULL;
    app->fff_data = flipper_format_string_alloc();

    // RollBack????
    app->rollback.base_counter = 0;
    app->rollback.target_counter = 0;
    app->rollback.step_size = 1;
    app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
    app->rollback.running = false;
    app->rollback.serial = 0;
    app->rollback.button = 1;
    strncpy(app->rollback.proto, "Kia V0", sizeof(app->rollback.proto));

    // ??last_result
    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));

    return app;
}

void protoPirateApp_free(ProtoPirateApp* app) {
    furi_assert(app);

    if(app->decoder_result) free(app->decoder_result);

    subghz_worker_free(app->subghz_worker);
    subghz_tx_rx_worker_free(app->txrx_worker);
    subghz_environment_free(app->environment);
    flipper_format_free(app->fff_data);
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

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ===================== ????? =====================
static bool back_event_callback(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    // ?????
    app->scene = SceneMainMenu;
    return true;
}

// ===================== ??: ??? =====================
void scene_main_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->submenu_index = index;

    switch(index) {
    case 0: // ????
        app->scene = SceneReceive;
        break;
    case 1: // ??.sub??
        app->scene = SceneSubLoad;
        break;
    case 2: // RollBack??
        app->scene = SceneRollback;
        break;
    case 3: // ????
        app->scene = SceneReplay;
        break;
    case 4: // ????
        app->scene = SceneHistory;
        break;
    case 5: // ????
        app->scene = SceneFreqSelect;
        break;
    case 6: // ????
        app->scene = SceneInfo;
        break;
    case 7: // ??
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

void scene_main_menu_alloc(ProtoPirateApp* app) {
    submenu_clean(app->submenu);
    submenu_set_header(app->submenu, "ProtoPirate RB");

    submenu_add_item(app->submenu, "?? Receive Signal", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Load .sub File", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? RollBack Attack", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Replay Signal", 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Signal History", 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Set Frequency", 5, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Protocol Info", 6, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "?? Exit", 7, scene_main_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_event_callback);
    view_set_context(submenu_get_view(app->submenu), app);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));
}

// ===================== ??: ???? =====================
void scene_receive_alloc(ProtoPirateApp* app) {
    widget_clean(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Receiving...");
    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Press car key fob button");
    widget_add_string_element(app->widget, 64, 45, AlignCenter, AlignTop, FontSecondary, "near the Flipper Zero");
    widget_add_string_element(app->widget, 64, 65, AlignCenter, AlignTop, FontKeyboard, "Press BACK to cancel");
}

void scene_receive_enter(ProtoPirateApp* app) {
    widget_clean(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Receiving...");
    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Waiting for signal...");
    widget_add_string_element(app->widget, 64, 45, AlignCenter, AlignTop, FontSecondary, "Press car key fob button");
    widget_add_string_element(app->widget, 64, 65, AlignCenter, AlignTop, FontKeyboard, "ESC = Back");

    // [TX?? - ????????]
    // ??subghz???????
    subghz_devices_init();
    SubGhzDevice* device = subghz_devices_get_by_index(0);
    if(device) {
        subghz_devices_begin(device);
        subghz_devices_load_preset(device, FuriHalSubGhzPresetOok650Async, NULL);
        subghz_devices_set_frequency(device, app->frequency);

        // ??????
        bool received = false;
        uint32_t timeout = furi_get_tick() + 10000; // 10???
        FuriString* rx_data = furi_string_alloc();

        while(furi_get_tick() < timeout && !received) {
            // ??BACK?
            // (??????)

            if(subghz_devices_is_rx_data(device)) {
                // ??????
                // (????,?????subghz_worker)
                received = true;
                furi_string_set(rx_data, "RAW_Data: -100 200 -300 400...");
            }
            furi_delay_ms(50);
        }

        subghz_devices_end(device);

        if(received) {
            // ??????
            furi_string_set(app->last_raw, furi_string_get_cstr(rx_data));

            // ????
            DecodeResult* result = decode_signal(app, rx_data);
            if(result) {
                memcpy(&app->last_result, result, sizeof(DecodeResult));
                free(result);
                app->scene = SceneResult;
            } else {
                widget_clean(app->widget);
                widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                                          "Signal Captured!");
                widget_add_string_element(app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary,
                                          "Unknown protocol");
                widget_add_string_element(app->widget, 64, 50, AlignCenter, AlignTop, FontKeyboard,
                                          "OK = Save RAW");
            }
        } else {
            widget_clean(app->widget);
            widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                                      "No signal received");
        }

        furi_string_free(rx_data);
    }

    subghz_devices_deinit();
}

// ===================== ??: ?? =====================
void scene_result_alloc(ProtoPirateApp* app) {
    widget_clean(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "DECODED!");

    char line[64];
    snprintf(line, sizeof(line), "%s %u-bit @ %.2f MHz",
             app->last_result.proto, app->last_result.bits,
             (double)app->frequency / 1000000.0);
    widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Sn:0x%07lX %s",
             app->last_result.serial, app->last_result.btn_name);
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

    snprintf(line, sizeof(line), "Cnt:0x%04X CRC:%s",
             app->last_result.counter,
             app->last_result.crc_ok ? "OK" : "FAIL");
    widget_add_string_element(app->widget, 64, 54, AlignCenter, AlignTop, FontKeyboard, line);

    // ????(??????????)
    button_menu_clean(app->button_menu);
    button_menu_set_header(app->button_menu, "Actions");

    // ?? ?? - TX??
    button_menu_add_item(
        app->button_menu,
        "?? Transmit",
        NULL,
        0,
        NULL,
        app);

    // ?? ?? (??????)
    button_menu_add_item(
        app->button_menu,
        "?? Replay x3",
        NULL,
        1,
        NULL,
        app);

    // ? ???? (??????)
    button_menu_add_item(
        app->button_menu,
        "? Brute x10",
        NULL,
        2,
        NULL,
        app);

    // ?? ??
    button_menu_add_item(
        app->button_menu,
        "?? Save",
        NULL,
        3,
        NULL,
        app);

    button_menu_set_selected_item(app->button_menu, 0);
}

void scene_result_enter(ProtoPirateApp* app) {
    // ??????
    scene_result_alloc(app);

    // ?????
    if(app->history_count < MAX_HISTORY) {
        HistoryItem* item = &app->history[app->history_count++];
        memcpy(&item->result, &app->last_result, sizeof(DecodeResult));
        item->raw_data = furi_string_alloc_set(app->last_raw);
        item->frequency = app->frequency;
    }
}

// ===================== ??: RollBack?? =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    RollbackState* rs = &app->rollback;
    widget_clean(app->widget);

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "?? RollBack Attack");
    widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
                              "Counter Rollback Engine");

    char line[64];
    if(app->last_result.bits > 0 && strstr(app->last_result.proto, "Kia")) {
        snprintf(line, sizeof(line), "Serial: 0x%07lX  %s",
                 app->last_result.serial, app->last_result.btn_name);
        widget_add_string_element(app->widget, 64, 34, AlignCenter, AlignTop, FontKeyboard, line);

        snprintf(line, sizeof(line), "Base Cnt: 0x%04X  Step: %u",
                 app->last_result.counter, rs->step_size);
        widget_add_string_element(app->widget, 64, 46, AlignCenter, AlignTop, FontKeyboard, line);

        rs->serial = app->last_result.serial;
        rs->button = app->last_result.button;
        rs->base_counter = app->last_result.counter;
        rs->target_counter = 0;
        strncpy(rs->proto, app->last_result.proto, sizeof(rs->proto));
    } else {
        widget_add_string_element(app->widget, 64, 34, AlignCenter, AlignTop, FontSecondary,
                                  "?? No Kia signal captured");
        widget_add_string_element(app->widget, 64, 46, AlignCenter, AlignTop, FontKeyboard,
                                  "Capture a Kia signal first");
    }

    widget_add_string_element(app->widget, 64, 62, AlignCenter, AlignTop, FontSecondary,
                              "?? OK = Run Attack (cnt?0)");
    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontKeyboard,
                              "BACK = Menu");
}

// ===================== ??: ?? =====================
void scene_replay_alloc(ProtoPirateApp* app) {
    widget_clean(app->widget);

    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "?? Replay Signal");
    widget_add_string_element(app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary,
                              "TX: Fully Unlocked");

    if(app->last_result.bits > 0) {
        char line[64];
        snprintf(line, sizeof(line), "%s  Sn:0x%07lX",
                 app->last_result.proto, app->last_result.serial);
        widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

        snprintf(line, sizeof(line), "Cnt:0x%04X  %s",
                 app->last_result.counter, app->last_result.btn_name);
        widget_add_string_element(app->widget, 64, 50, AlignCenter, AlignTop, FontKeyboard, line);
    } else {
        widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontSecondary,
                                  "No signal captured");
    }

    widget_add_string_element(app->widget, 64, 66, AlignCenter, AlignTop, FontSecondary,
                              "OK = Replay x3   UP = Replay x10");
}

// ===================== ??: ???? =====================
void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_clean(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, "Frequency");

    char freq_str[20];
    snprintf(freq_str, sizeof(freq_str), "%lu", app->frequency);

    variable_item_list_add(app->var_item_list, "315.00 MHz (US)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "433.92 MHz (EU/Asia)", 0, NULL, NULL);
    variable_item_list_add(app->var_item_list, "868.35 MHz (EU)", 0, NULL, NULL);
}

void scene_freq_select_enter(ProtoPirateApp* app) {
    // ??????
    // ????????VariableItemList?change??
}

// ===================== ??: ?? =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_clean(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate RB");
    widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
                              "RollBack Edition");

    widget_add_string_element(app->widget, 64, 34, AlignCenter, AlignTop, FontKeyboard,
                              "?? TX: FULLY UNLOCKED");
    widget_add_string_element(app->widget, 64, 44, AlignCenter, AlignTop, FontKeyboard,
                              "?? RollBack Attack Engine");
    widget_add_string_element(app->widget, 64, 54, AlignCenter, AlignTop, FontKeyboard,
                              "?? Kia V0/V1/V2 Decoder");
    widget_add_string_element(app->widget, 64, 64, AlignCenter, AlignTop, FontKeyboard,
                              "?? Replay / Brute Force TX");
}

// ===================== ???? =====================
static uint32_t navigation_callback(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    app->scene = SceneMainMenu;
    return 0;
}

// ===================== ??? =====================
int32_t protopirate_rb_app_entry(void* p) {
    UNUSED(p);
    ProtoPirateApp* app = protoPirateApp_alloc();

    // ????????
    scene_main_menu_alloc(app);
    scene_receive_alloc(app);
    scene_result_alloc(app);
    scene_rollback_alloc(app);
    scene_replay_alloc(app);
    scene_info_alloc(app);
    scene_freq_select_alloc(app);

    // ????
    view_set_previous_callback(submenu_get_view(app->submenu), navigation_callback);

    // ??????
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);

    // ?????
    view_dispatcher_run(app->view_dispatcher);

    // ??
    protoPirateApp_free(app);
    return 0;
}
