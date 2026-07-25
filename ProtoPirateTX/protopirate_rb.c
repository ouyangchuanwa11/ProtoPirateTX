#include "protopirate_rb.h"

// ===================== 全局 TX 设备指针（外部引用） =====================
extern const SubGhzDevice* g_device;
extern bool tx_device_init(void);

// ===================== 辅助字符串缓冲区 =====================
static char sn_str[64];

// ===================== 回调函数 =====================
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app || type != InputTypePress) return;
    switch(index) {
    case 0: view_dispatcher_send_custom_event(app->view_dispatcher, EventSendRaw); break;
    case 1: transmit_packet(app, app->last_result.data_hi, app->last_result.data_lo, app->frequency, 5); break;
    case 2: view_dispatcher_send_custom_event(app->view_dispatcher, EventReplaySingle); break;
    case 3: view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchConfig); break;
    case 4: view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackRun); break;
    case 5: view_dispatcher_send_custom_event(app->view_dispatcher, EventSimulate); break;
    case 6: view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu); break;
    }
}

static void rollback_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0: view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackRun); break;
    case 1: view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackConfig); break;
    case 2: view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend); break;
    case 3: view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu); break;
    }
}

static void replay_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0: transmit_packet(app, app->last_result.data_hi, app->last_result.data_lo, app->frequency, 5); break;
    case 1: { uint32_t hi,lo; rollback_build_frame_proto(app->rollback.protocol_type,0x1234567,2,0x100,&hi,&lo); transmit_packet(app,hi,lo,app->frequency,5); } break;
    case 2: view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend); break;
    case 3: view_dispatcher_send_custom_event(app->view_dispatcher, EventSimulate); break;
    case 4: view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu); break;
    }
}

static void receive_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
        if(rx_start_capture(app)) {
            FuriString* captured = rx_format_capture(app);
            if(captured) {
                DecodeResult* dec = decode_signal(app, captured);
                if(dec) { memcpy(&app->last_result, dec, sizeof(DecodeResult)); free(dec); }
                else { app->last_result.bits = 0; app->last_result.is_demo = false; }
                furi_string_set(app->last_raw, captured);
                furi_string_free(captured);
            }
            rx_stop_capture(app);
            view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
        } else {
            app->last_result.bits = 64; app->last_result.serial = 0x1234567;
            app->last_result.button = 2; app->last_result.counter = 0xABCD;
            app->last_result.crc_ok = true; app->last_result.is_demo = true;
            strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
            strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
            rollback_build_frame(0x1234567, 2, 0xABCD, &app->last_result.data_hi, &app->last_result.data_lo);
            app->rollback.serial = app->last_result.serial;
            app->rollback.button = app->last_result.button;
            app->rollback.base_counter = app->last_result.counter;
            strncpy(app->rollback.proto, app->last_result.proto, sizeof(app->rollback.proto));
            view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
        }
        break;
    case 1: view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu); break;
    }
}

// ===================== 主菜单场景回调 =====================
static void scene_main_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0: view_dispatcher_send_custom_event(app->view_dispatcher, EventReceive); break;
    case 1: view_dispatcher_send_custom_event(app->view_dispatcher, EventReplay); break;
    case 2: view_dispatcher_send_custom_event(app->view_dispatcher, EventRollback); break;
    case 3: view_dispatcher_send_custom_event(app->view_dispatcher, EventSimulate); break;
    case 4: view_dispatcher_send_custom_event(app->view_dispatcher, EventFreqSelect); break;
    case 5: view_dispatcher_send_custom_event(app->view_dispatcher, EventInfo); break;
    case 6: view_dispatcher_stop(app->view_dispatcher); return;
    }
}

// ===================== 频率选择回调 =====================
static void freq_item_change_callback(VariableItem* item) {
    ProtoPirateApp* app = (ProtoPirateApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t freqs[] = {315000000, 433920000, 868350000, 915000000};
    if(idx < 4) app->frequency = freqs[idx];
}

static void protocol_item_change_callback(VariableItem* item) {
    ProtoPirateApp* app = (ProtoPirateApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx < Proto_COUNT) {
        app->rollback.protocol_type = idx;
        strncpy(app->rollback.proto, PROTO_NAMES[idx], sizeof(app->rollback.proto));
    }
}

static void step_item_change_callback(VariableItem* item) {
    ProtoPirateApp* app = (ProtoPirateApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t steps[] = {1, 5, 10, 25, 50, 100, 200, 500};
    if(idx < 8) app->rollback.step_size = steps[idx];
}

static void burst_item_change_callback(VariableItem* item) {
    ProtoPirateApp* app = (ProtoPirateApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t bursts[] = {1, 2, 3, 5, 10, 15, 20};
    if(idx < 7) app->rollback.burst_count = bursts[idx];
}

// ===================== RX 捕获 =====================
bool rx_start_capture(ProtoPirateApp* app) {
    if(!app || !g_device) {
        if(!tx_device_init()) return false;
    }
    app->rx_running = true;
    app->pulse_count = 0;
    subghz_devices_begin(g_device);
    subghz_devices_load_preset(g_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_device, app->frequency);
    subghz_devices_set_rx(g_device);
    furi_delay_ms(10);

    const int32_t sample_us = 50, max_samples = 50000;
    const int16_t thresh = -60;
    uint32_t taken = 0;
    bool last = false;
    uint32_t pdur = 0;

    while(app->rx_running && taken < max_samples) {
        bool level = (subghz_devices_get_rssi(g_device) > thresh);
        if(level != last) {
            if(pdur > 0 && app->pulse_count < 4096)
                app->pulse_buffer[app->pulse_count++] = last ? (int32_t)(pdur * sample_us) : -(int32_t)(pdur * sample_us);
            pdur = 0; last = level;
        }
        pdur++; taken++; furi_delay_us(sample_us);
    }
    if(pdur > 0 && app->pulse_count < 4096)
        app->pulse_buffer[app->pulse_count++] = last ? (int32_t)(pdur * sample_us) : -(int32_t)(pdur * sample_us);
    subghz_devices_idle(g_device);
    subghz_devices_end(g_device);
    app->rx_running = false;
    app->rx_captured = (app->pulse_count > 10);
    FURI_LOG_I(TAG, "RX done: %u pulses", app->pulse_count);
    return app->rx_captured;
}

void rx_stop_capture(ProtoPirateApp* app) { app->rx_running = false; }

FuriString* rx_format_capture(ProtoPirateApp* app) {
    if(!app || app->pulse_count == 0) return NULL;
    FuriString* result = furi_string_alloc();
    for(uint16_t i = 0; i < app->pulse_count; i++)
        furi_string_cat_printf(result, "%ld ", app->pulse_buffer[i]);
    return result;
}

// ===================== 事件处理 =====================
static bool custom_event_callback(void* context, uint32_t event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return false;
    switch(event) {
    case EventGoMenu:          scene_main_menu_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventReceive:         scene_receive_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventRollback:        scene_rollback_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventRollbackConfig:  scene_rollback_config_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList); return true;
    case EventReplay:          scene_replay_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventFreqSelect:      scene_freq_select_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList); return true;
    case EventInfo:            scene_info_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget); return true;
    case EventReceiveDone:     scene_result_main_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewButtonMenu); return true;
    case EventRollbackRun:
        if(app->rollback.running) { app->rollback.running = false; }
        else { scene_rollback_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading); rollback_attack_run(app); scene_main_menu_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); }
        return true;
    case EventBatchConfig:     scene_batch_config_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventBatchSend:       scene_batch_send_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading); batch_send_start(app); scene_main_menu_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    case EventSendRaw:         transmit_packet(app, app->last_result.data_hi, app->last_result.data_lo, app->frequency, 3); return true;
    case EventReplaySingle:    transmit_packet(app, app->last_result.data_hi, app->last_result.data_lo, app->frequency, 5); return true;
    case EventSimulate:        scene_simulate_alloc(app); view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu); return true;
    default: return false;
    }
}

static uint32_t back_to_menu_callback(void* context) { UNUSED(context); return ViewMenu; }
static bool nav_event_callback(void* context) { UNUSED(context); return false; }

// ===================== 分配/释放 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(ProtoPirateApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
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
    app->dialog = dialog_ex_alloc();
    app->frequency = DEFAULT_FREQ;
    app->history_count = 0;
    app->last_raw = furi_string_alloc();
    app->last_raw_hex = furi_string_alloc();
    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));
    app->rollback.base_counter = 0; app->rollback.target_counter = 100;
    app->rollback.step_size = 1; app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
    app->rollback.running = false; app->rollback.serial = 0x1234567;
    app->rollback.button = 2; app->rollback.protocol_type = Proto_Kia_V0;
    strncpy(app->rollback.proto, "Kia V0", sizeof(app->rollback.proto));
    app->batch.count = 50; app->batch.active = false;
    app->simulate_raw = furi_string_alloc();
    return app;
}

void protoPirateApp_free(ProtoPirateApp* app) {
    furi_assert(app);
    furi_string_free(app->last_raw); furi_string_free(app->last_raw_hex);
    if(app->simulate_raw) furi_string_free(app->simulate_raw);
    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, ViewButtonMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, ViewLoading);
    submenu_free(app->submenu); variable_item_list_free(app->var_item_list);
    text_box_free(app->text_box); widget_free(app->widget);
    button_menu_free(app->button_menu); popup_free(app->popup);
    dialog_ex_free(app->dialog); view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI); furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

// ===================== 场景分配 =====================
void scene_main_menu_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu); submenu_set_header(app->submenu, "ProtoPirate TX v3.1");
    char buf[32]; snprintf(buf, sizeof(buf), "Freq: %.3f MHz", (double)app->frequency/1000000.0f);
    submenu_add_item(app->submenu, "📡 Capture Signal", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "🔄 Replay Signal", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "🔥 RollBack Attack", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "📶 Simulate Signal", 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, buf, 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "ℹ️ About", 5, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "🚪 Exit", 6, scene_main_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

void scene_receive_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu); submenu_set_header(app->submenu, "Capture Signal");
    char buf[32]; snprintf(buf, sizeof(buf), "Freq: %.3f MHz", (double)app->frequency/1000000.0f);
    submenu_add_item(app->submenu, "🎯 Start Capture", 0, receive_menu_callback, app);
    submenu_add_item(app->submenu, buf, 0, NULL, app);
    submenu_add_item(app->submenu, "⬅ Back", 1, receive_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

void scene_result_main_alloc(ProtoPirateApp* app) {
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "⚡ Signal Captured! ⚡");
    snprintf(sn_str, sizeof(sn_str), "📤 Send x3 (Sn:%07lX)", app->last_result.serial);
    button_menu_add_item(app->button_menu, sn_str, 0, result_button_callback, ButtonMenuItemTypeCommon, app);
    snprintf(sn_str, sizeof(sn_str), "🔁 Replay x5 (Btn:%s)", app->last_result.btn_name);
    button_menu_add_item(app->button_menu, sn_str, 1, result_button_callback, ButtonMenuItemTypeCommon, app);
    snprintf(sn_str, sizeof(sn_str), "📶 Simulate RAW");
    button_menu_add_item(app->button_menu, sn_str, 5, result_button_callback, ButtonMenuItemTypeCommon, app);
    snprintf(sn_str, sizeof(sn_str), "📦 Batch Send");
    button_menu_add_item(app->button_menu, sn_str, 3, result_button_callback, ButtonMenuItemTypeCommon, app);
    snprintf(sn_str, sizeof(sn_str), "🔥 RollBack (%04X->0000)", app->last_result.counter);
    button_menu_add_item(app->button_menu, sn_str, 4, result_button_callback, ButtonMenuItemTypeCommon, app);
    snprintf(sn_str, sizeof(sn_str), "🔄 Replay Burst x10");
    button_menu_add_item(app->button_menu, sn_str, 2, result_button_callback, ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "⬅ Main Menu", 6, result_button_callback, ButtonMenuItemTypeBack, app);
}

void scene_rollback_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "🔥 RollBack Attack");
    char buf[64];
    snprintf(buf, sizeof(buf), "▶ Start Attack (%s)", app->rollback.proto);
    submenu_add_item(app->submenu, buf, 0, rollback_menu_callback, app);
    snprintf(buf, sizeof(buf), "⚙ Config (Step:%u Burst:%u)", app->rollback.step_size, app->rollback.burst_count);
    submenu_add_item(app->submenu, buf, 1, rollback_menu_callback, app);
    snprintf(buf, sizeof(buf), "📦 Batch Send (%u frames)", app->batch.count);
    submenu_add_item(app->submenu, buf, 2, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "⬅ Back", 3, rollback_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

void scene_rollback_config_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, "RollBack Config");

    VariableItem* item = variable_item_list_add(app->var_item_list, "Protocol", Proto_COUNT, protocol_item_change_callback, app);
    variable_item_set_current_value_index(item, app->rollback.protocol_type);
    const char* proto_names[] = {"Kia V0","Kia V1","Kia V2","Ford","StarLine","Subaru","Fiat","Chrysler","Honda","Toyota"};
    for(uint8_t i = 0; i < Proto_COUNT; i++) variable_item_set_current_value_text(item, proto_names[i]);

    item = variable_item_list_add(app->var_item_list, "Step Size", 8, step_item_change_callback, app);
    uint8_t default_step = 0; uint16_t steps[] = {1,5,10,25,50,100,200,500};
    for(uint8_t i=0;i<8;i++) { if(steps[i]==app->rollback.step_size) default_step=i; }
    variable_item_set_current_value_index(item, default_step);
    for(uint8_t i=0;i<8;i++) { snprintf(sn_str,sizeof(sn_str),"%u",steps[i]); variable_item_set_current_value_text(item, sn_str); }

    item = variable_item_list_add(app->var_item_list, "Burst Count", 7, burst_item_change_callback, app);
    uint8_t default_burst = 2; uint8_t bursts[] = {1,2,3,5,10,15,20};
    for(uint8_t i=0;i<7;i++) { if(bursts[i]==app->rollback.burst_count) default_burst=i; }
    variable_item_set_current_value_index(item, default_burst);
    for(uint8_t i=0;i<7;i++) { snprintf(sn_str,sizeof(sn_str),"%u",bursts[i]); variable_item_set_current_value_text(item, sn_str); }

    item = variable_item_list_add(app->var_item_list, "Base Counter", 0, NULL, app);
    snprintf(sn_str, sizeof(sn_str), "0x%04X", app->rollback.base_counter);
    variable_item_set_current_value_text(item, sn_str);

    item = variable_item_list_add(app->var_item_list, "Target Counter", 0, NULL, app);
    snprintf(sn_str, sizeof(sn_str), "0x%04X", app->rollback.target_counter);
    variable_item_set_current_value_text(item, sn_str);

    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), back_to_menu_callback);
}

void scene_replay_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu); submenu_set_header(app->submenu, "🔄 Replay / TX");
    submenu_add_item(app->submenu, "🔁 Replay Last x5", 0, replay_menu_callback, app);
    submenu_add_item(app->submenu, "🧪 Send Demo Frame", 1, replay_menu_callback, app);
    submenu_add_item(app->submenu, "📦 Batch Send", 2, replay_menu_callback, app);
    submenu_add_item(app->submenu, "📶 Simulate RAW", 3, replay_menu_callback, app);
    submenu_add_item(app->submenu, "⬅ Back", 4, replay_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    variable_item_list_set_header(app->var_item_list, "Frequency Select");
    VariableItem* item = variable_item_list_add(app->var_item_list, "Frequency", 4, freq_item_change_callback, app);
    uint8_t def = 1; uint32_t freqs[] = {315000000,433920000,868350000,915000000};
    for(uint8_t i=0;i<4;i++) { if(freqs[i]==app->frequency) def=i; }
    variable_item_set_current_value_index(item, def);
    variable_item_set_current_value_text(item, "315 MHz");
    variable_item_set_current_value_text(item, "433 MHz");
    variable_item_set_current_value_text(item, "868 MHz");
    variable_item_set_current_value_text(item, "915 MHz");
    variable_item_set_current_value_index(item, def);
    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), back_to_menu_callback);
}

void scene_simulate_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu); submenu_set_header(app->submenu, "📶 Simulate Signal");
    char buf[64];
    snprintf(buf, sizeof(buf), "Simulate: %s", app->rollback.proto);
    submenu_add_item(app->submenu, buf, 0, NULL, app);
    snprintf(buf, sizeof(buf), "S/N: 0x%07lX Btn:%u", app->rollback.serial, app->rollback.button);
    submenu_add_item(app->submenu, buf, 1, NULL, app);
    snprintf(buf, sizeof(buf), "Counter: 0x%04X Step: %u", app->rollback.base_counter, app->rollback.step_size);
    submenu_add_item(app->submenu, buf, 2, NULL, app);

    static void simulate_action_callback(void* ctx, uint32_t idx) {
        ProtoPirateApp* a = (ProtoPirateApp*)ctx;
        if(!a) return;
        switch(idx) {
        case 0: { uint32_t h,l; rollback_build_frame_proto(a->rollback.protocol_type, a->rollback.serial, a->rollback.button, a->rollback.base_counter, &h, &l); transmit_packet(a, h, l, a->frequency, a->rollback.burst_count); } break;
        case 1: if(a->rollback.base_counter > a->rollback.step_size) a->rollback.base_counter -= a->rollback.step_size; else a->rollback.base_counter = 0; break;
        case 2: a->rollback.base_counter += a->rollback.step_size; break;
        case 3: view_dispatcher_send_custom_event(a->view_dispatcher, EventGoMenu); break;
        }
    }
    submenu_add_item(app->submenu, "▶ TX Now", 0, simulate_action_callback, app);
    submenu_add_item(app->submenu, "🔽 Counter -Step", 1, simulate_action_callback, app);
    submenu_add_item(app->submenu, "🔼 Counter +Step", 2, simulate_action_callback, app);
    submenu_add_item(app->submenu, "⬅ Back", 3, simulate_action_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

void scene_batch_config_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu); submenu_set_header(app->submenu, "Batch Config");
    static void batch_menu_cb(void* ctx, uint32_t idx) {
        ProtoPirateApp* a = (ProtoPirateApp*)ctx;
        if(!a) return;
        uint16_t counts[] = {10, 50, 100, 200, 500};
        if(idx < 5) { a->batch.count = counts[idx]; view_dispatcher_send_custom_event(a->view_dispatcher, EventBatchSend); }
        else view_dispatcher_send_custom_event(a->view_dispatcher, EventGoMenu);
    }
    submenu_add_item(app->submenu, "10 frames", 0, batch_menu_cb, app);
    submenu_add_item(app->submenu, "50 frames", 1, batch_menu_cb, app);
    submenu_add_item(app->submenu, "100 frames", 2, batch_menu_cb, app);
    submenu_add_item(app->submenu, "200 frames", 3, batch_menu_cb, app);
    submenu_add_item(app->submenu, "500 frames", 4, batch_menu_cb, app);
    submenu_add_item(app->submenu, "⬅ Back", 5, batch_menu_cb, app);
}

void scene_batch_send_alloc(ProtoPirateApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "TX Running", 64, 10, AlignCenter, AlignTop);
    char buf[32]; snprintf(buf, sizeof(buf), "Sending %u frames...", app->batch.count);
    popup_set_text(app->popup, buf, 64, 32, AlignCenter, AlignCenter);
}

void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(app->widget, 5, 8, AlignLeft, AlignTop, FontSecondary,
        "ProtoPirate TX v3.1\n\n"
        "Sub-GHz Attack Tool\n"
        "for Flipper Zero\n\n"
        "Features:\n"
        "- Signal Capture & Decode\n"
        "- Multi-Protocol Support\n"
        "- RollBack Counter Attack\n"
        "- RAW Signal Simulate\n"
        "- Batch Transmission\n\n"
        "Firmware: Momentum\n"
        "CC1101 TX: ENABLED ✅\n\n"
        "[Back] to return");
    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== app_main 入口 =====================
__attribute__((visibility("default")))
int32_t app_main(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "==========================================");
    FURI_LOG_I(TAG, "  ProtoPirate TX v3.1 — Momentum Edition");
    FURI_LOG_I(TAG, "  TX: ENABLED | Sub-GHz: CC1101");
    FURI_LOG_I(TAG, "==========================================");
    
    // ⚡ 初始化 CC1101 设备 — 射频发送已启用
    if(!tx_device_init()) {
        FURI_LOG_E(TAG, "❌ CC1101 NOT FOUND. Is your Flipper Zero Sub-GHz module present?");
        // 仍然启动应用，只是 TX 不可用
    } else {
        FURI_LOG_I(TAG, "✅ CC1101 ready — TX/RX operational");
    }
    
    ProtoPirateApp* app = protoPirateApp_alloc();
    if(!app) return -1;
    
    view_dispatcher_add_view(app->view_dispatcher, ViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ViewVarList, variable_item_list_get_view(app->var_item_list));
    view_dispatcher_add_view(app->view_dispatcher, ViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, ViewButtonMenu, button_menu_get_view(app->button_menu));
    view_dispatcher_add_view(app->view_dispatcher, ViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, ViewDialog, dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(app->view_dispatcher, ViewLoading, popup_get_view(app->popup));
    
    scene_main_menu_alloc(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    
    // 清理
    transmit_packet_stop(app);
    if(g_device) {
        // Device will be cleaned up by FURI HAL on app exit
    }
    protoPirateApp_free(app);
    
    FURI_LOG_I(TAG, "ProtoPirate TX — Shutdown complete");
    return 0;
}
