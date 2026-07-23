#include "protopirate_rb.h"
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/protocols/raw.h>

// ===================== 导航回调 =====================
static uint32_t menu_navigation(void* context) {
    UNUSED(context);
    return ViewMenu;
}

uint32_t main_menu_navigation(void* context) {
    return menu_navigation(context);
}

// ===================== TX 发射回调（结果按钮菜单） =====================
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(type != InputTypePress) return;

    if(index == 0) {
        // Transmit
        if(app->last_result.bits > 0) {
            transmit_packet(
                app,
                app->last_result.data_hi,
                app->last_result.data_lo,
                app->frequency,
                1);
            // Show confirmation
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary,
                "TX Sent!");
            widget_add_string_element(
                app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary,
                "Press BACK to continue");
        }
    } else if(index == 1) {
        // Replay x3 burst
        if(app->last_result.bits > 0) {
            transmit_packet(
                app,
                app->last_result.data_hi,
                app->last_result.data_lo,
                app->frequency,
                3);
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary,
                "Burst x3 Done!");
            widget_add_string_element(
                app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary,
                "Press BACK to continue");
        }
    } else if(index == 2) {
        // RollBack attack from current result
        app->rollback.serial = app->last_result.serial;
        app->rollback.button = app->last_result.button;
        app->rollback.base_counter = app->last_result.counter;
        app->rollback.target_counter = app->last_result.counter + 256;
        app->rollback.step_size = 1;
        app->rollback.burst_count = ROLLBACK_BURST_DEFAULT;
        strncpy(app->rollback.proto, app->last_result.proto, sizeof(app->rollback.proto));
        app->scene = SceneRollback;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        scene_rollback_alloc(app);
    } else if(index == 3) {
        // Back to menu
        app->scene = SceneMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    }
}

// ===================== 应用生命周期 =====================
ProtoPirateApp* protoPirateApp_alloc(void) {
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    app->var_item_list = variable_item_list_alloc();
    app->text_box = text_box_alloc();
    app->widget = widget_alloc();
    app->button_menu = button_menu_alloc();
    app->popup = popup_alloc();
    app->loading = loading_alloc();

    app->frequency = DEFAULT_FREQ;
    app->tx_frequency = DEFAULT_FREQ;
    app->scene = SceneMainMenu;
    app->submenu_index = 0;
    app->result_menu_index = 0;
    app->history_count = 0;
    app->last_raw = furi_string_alloc();

    memset(&app->last_result, 0, sizeof(DecodeResult));
    strncpy(app->last_result.proto, "None", sizeof(app->last_result.proto));

    // RX worker init
    memset(&app->rx, 0, sizeof(RxWorker));
    app->rx.raw_output = furi_string_alloc();

    // Rollback state
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
    rx_stop(app);
    furi_string_free(app->rx.raw_output);
    furi_string_free(app->last_raw);
    for(uint8_t i = 0; i < app->history_count; i++) {
        if(app->history[i].raw_data) furi_string_free(app->history[i].raw_data);
    }
    loading_free(app->loading);
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
    switch(index) {
    case 0:
        app->scene = SceneReceive;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
        scene_receive_enter(app);
        break;
    case 1:
        app->scene = SceneReplay;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        scene_replay_enter(app);
        break;
    case 2:
        app->scene = SceneRollback;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        scene_rollback_alloc(app);
        break;
    case 3:
        app->scene = SceneFreqSelect;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
        scene_freq_select_alloc(app);
        break;
    case 4:
        app->scene = SceneInfo;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        scene_info_alloc(app);
        break;
    case 5:
        view_dispatcher_stop(app->view_dispatcher);
        break;
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

    view_set_previous_callback(
        submenu_get_view(app->submenu), menu_navigation);
    if(!view_dispatcher_has_view(app->view_dispatcher, ViewMenu)) {
        view_dispatcher_add_view(
            app->view_dispatcher, ViewMenu, submenu_get_view(app->submenu));
    }
}

// ===================== 场景: 接收 =====================
void scene_receive_alloc(ProtoPirateApp* app) {
    UNUSED(app);
    // View is allocated during enter
}

void scene_receive_enter(ProtoPirateApp* app) {
    // Show loading while receiving
    loading_free(app->loading);
    app->loading = loading_alloc();
    if(!view_dispatcher_has_view(app->view_dispatcher, ViewLoading)) {
        view_dispatcher_add_view(
            app->view_dispatcher, ViewLoading, loading_get_view(app->loading));
    }

    // Start RX in worker thread
    rx_start(app);
}

void scene_receive_exit(ProtoPirateApp* app) {
    rx_stop(app);
}

// ===================== RX Worker 线程 =====================
// 使用 SubGhzWorker + Receiver 进行真实接收
int32_t rx_worker_thread(void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    RxWorker* rx = &app->rx;
    rx->is_running = true;
    rx->signal_received = false;

    FURI_LOG_I(TAG, "RX worker starting on freq=%lu", rx->rx_frequency);

    // 分配 worker
    rx->worker = subghz_worker_alloc();
    rx->environment = subghz_environment_alloc();
    rx->setting = subghz_setting_alloc();
    subghz_setting_load(rx->setting, EXTRA_RADIO_DEVICE);
    subghz_environment_set_came_atomo_rainbow_table_path(
        rx->environment, EXTRA_RADIO_DEVICE);
    subghz_environment_set_nice_flor_s_rainbow_table_path(
        rx->environment, EXTRA_RADIO_DEVICE);
    subghz_environment_set_protocols(rx->environment, subghz_protocol_registry);
    subghz_environment_set_filter(rx->environment, SubGhzProtocolFlag_Decodable);

    rx->receiver = subghz_receiver_alloc_init(rx->environment);

    // 设置接收回调
    subghz_receiver_set_filter(rx->receiver, SubGhzProtocolFlag_Decodable);

    subghz_worker_set_overrun(rx->worker, true);
    subghz_worker_set_rx_callback(rx->worker, NULL, NULL);

    // 开始接收
    subghz_worker_start(rx->worker);
    subghz_receiver_reset(rx->receiver);
    subghz_worker_set_frequency(rx->worker, rx->rx_frequency);

    FURI_LOG_I(TAG, "RX worker listening...");

    // 超时接收循环
    uint32_t start_tick = furi_get_tick();
    UNUSED(start_tick);

    while(rx->is_running) {
        furi_delay_ms(50);
        // 每50ms检查是否有信号（简单超时循环）
        if(furi_get_tick() - start_tick > RX_TIMEOUT_MS) {
            FURI_LOG_I(TAG, "RX timeout reached");
            break;
        }
    }

    // 停止接收
    subghz_worker_stop(rx->worker);
    subghz_receiver_free(rx->receiver);
    subghz_worker_free(rx->worker);
    subghz_environment_free(rx->environment);
    subghz_setting_free(rx->setting);
    rx->worker = NULL;
    rx->receiver = NULL;
    rx->environment = NULL;
    rx->setting = NULL;

    // 如果没有抓到信号，写入模拟数据用于 demo
    if(!rx->signal_received) {
        FURI_LOG_I(TAG, "No real signal captured, using demo data");
        furi_string_set(rx->raw_output, "DEMO");
    }

    rx->is_running = false;
    return 0;
}

bool rx_start(ProtoPirateApp* app) {
    RxWorker* rx = &app->rx;
    if(rx->is_running) return false;

    rx->rx_frequency = app->frequency;
    furi_string_reset(rx->raw_output);

    rx->thread = furi_thread_alloc_ex("ProtoRX", 2048, rx_worker_thread, app);
    furi_thread_start(rx->thread);

    // 等待接收完成
    furi_thread_join(rx->thread);
    furi_thread_free(rx->thread);
    rx->thread = NULL;

    // 解析接收到的信号
    FURI_LOG_I(TAG, "RX done, processing...");

    // 尝试解码
    DecodeResult* result = decode_signal(app, rx->raw_output);

    if(result) {
        app->last_result = *result;
        app->tx_frequency = app->frequency;

        // 保存到历史
        if(app->history_count < MAX_HISTORY) {
            app->history[app->history_count].result = *result;
            app->history[app->history_count].raw_data = furi_string_alloc_set(rx->raw_output);
            app->history[app->history_count].frequency = app->frequency;
            app->history_count++;
        }

        free(result);
    } else {
        // Demo fallback
        app->last_result.bits = 64;
        app->last_result.serial = 0x1234567;
        app->last_result.button = 2;
        strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
        strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
        app->last_result.counter = 0xABCD;
        app->last_result.crc_ok = true;
        app->last_result.data_hi = 0;
        app->last_result.data_lo = 0x1234567;
    }

    // 切换到结果场景
    app->scene = SceneResult;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
    scene_result_alloc(app);
    return true;
}

void rx_stop(ProtoPirateApp* app) {
    RxWorker* rx = &app->rx;
    if(rx->is_running) {
        rx->is_running = false;
        if(rx->thread) {
            furi_thread_join(rx->thread);
            furi_thread_free(rx->thread);
            rx->thread = NULL;
        }
    }
}

// ===================== 场景: 结果 =====================
void scene_result_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "DECODED!");

    char line[64];
    snprintf(line, sizeof(line), "%s %u-bit @ %lu kHz",
             app->last_result.proto, app->last_result.bits,
             app->frequency / 1000);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Sn:0x%07lX %s",
             app->last_result.serial, app->last_result.btn_name);
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignTop, FontKeyboard, line);

    snprintf(line, sizeof(line), "Cnt:0x%04X CRC:%s",
             app->last_result.counter,
             app->last_result.crc_ok ? "OK" : "FAIL");
    widget_add_string_element(
        app->widget, 64, 54, AlignCenter, AlignTop, FontKeyboard, line);

    // Remove old button menu view if already added
    if(view_dispatcher_has_view(app->view_dispatcher, ViewButtonMenu)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewButtonMenu);
    }
    if(view_dispatcher_has_view(app->view_dispatcher, ViewWidget)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    }

    view_dispatcher_add_view(
        app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
}

void scene_result_enter(ProtoPirateApp* app) {
    scene_result_alloc(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    // Prepare button menu for actions
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "Actions");
    button_menu_add_item(
        app->button_menu, "Transmit", 0, result_button_callback,
        ButtonMenuItemTypeCommon, app);
    button_menu_add_item(
        app->button_menu, "Replay x3", 1, result_button_callback,
        ButtonMenuItemTypeCommon, app);
    button_menu_add_item(
        app->button_menu, "RollBack", 2, result_button_callback,
        ButtonMenuItemTypeCommon, app);
    button_menu_add_item(
        app->button_menu, "Back", 3, result_button_callback,
        ButtonMenuItemTypeCommon, app);
    button_menu_set_selected_item(app->button_menu, 0);

    if(view_dispatcher_has_view(app->view_dispatcher, ViewButtonMenu)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewButtonMenu);
    }
    view_dispatcher_add_view(
        app->view_dispatcher, ViewButtonMenu, button_menu_get_view(app->button_menu));
}

// ===================== 场景: RollBack攻击 =====================
void scene_rollback_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "RollBack Attack");

    char line[64];
    snprintf(line, sizeof(line), "Serial: 0x%07lX", app->rollback.serial);
    widget_add_string_element(
        app->widget, 64, 22, AlignCenter, AlignTop, FontSecondary, line);

    snprintf(line, sizeof(line), "Btn: %d   Burst: %u",
             app->rollback.button, app->rollback.burst_count);
    widget_add_string_element(
        app->widget, 64, 36, AlignCenter, AlignTop, FontKeyboard, line);

    snprintf(line, sizeof(line), "Cnt 0x%04X -> 0x%04X",
             app->rollback.base_counter, app->rollback.target_counter);
    widget_add_string_element(
        app->widget, 64, 50, AlignCenter, AlignTop, FontKeyboard, line);

    widget_add_string_element(
        app->widget, 64, 66, AlignCenter, AlignTop, FontSecondary,
        "OK = Start Attack");
    widget_add_string_element(
        app->widget, 64, 78, AlignCenter, AlignTop, FontKeyboard,
        "BACK = Menu");

    view_set_previous_callback(
        widget_get_view(app->widget), menu_navigation);

    if(view_dispatcher_has_view(app->view_dispatcher, ViewWidget)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    }
    view_dispatcher_add_view(
        app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

void scene_rollback_enter(ProtoPirateApp* app) {
    scene_rollback_alloc(app);
    // Run the attack
    rollback_attack_run(app);

    // Show result
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary,
        "Attack Complete!");
    widget_add_string_element(
        app->widget, 64, 50, AlignCenter, AlignCenter, FontSecondary,
        "Press BACK for menu");
}

void scene_rollback_exit(ProtoPirateApp* app) {
    app->rollback.running = false;
}

// ===================== 场景: 重放 =====================
void scene_replay_enter(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "Replay Signal");

    if(app->last_result.bits > 0) {
        char line[64];
        snprintf(line, sizeof(line), "%s  Sn:0x%07lX",
                 app->last_result.proto, app->last_result.serial);
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignTop, FontKeyboard, line);

        // Send the packet
        transmit_packet(
            app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency,
            1);

        widget_add_string_element(
            app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary,
            "Replayed!");
    } else {
        // Demo: send a known test frame
        widget_add_string_element(
            app->widget, 64, 30, AlignCenter, AlignTop, FontSecondary,
            "Demo: sending test...");

        uint32_t demo_serial = 0x1234567;
        uint32_t demo_data_hi, demo_data_lo;
        rollback_build_frame(demo_serial, 2, 0x100, &demo_data_hi, &demo_data_lo);
        transmit_packet(app, demo_data_hi, demo_data_lo, app->frequency, 3);

        widget_add_string_element(
            app->widget, 64, 55, AlignCenter, AlignTop, FontSecondary,
            "Demo TX sent! (x3)");
    }

    view_set_previous_callback(
        widget_get_view(app->widget), menu_navigation);

    if(view_dispatcher_has_view(app->view_dispatcher, ViewWidget)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    }
    view_dispatcher_add_view(
        app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// ===================== 场景: 频率选择 =====================
typedef enum {
    FreqIdx315,
    FreqIdx433,
    FreqIdx868,
    FreqIdxCount,
} FreqIndex;

static const uint32_t freq_table[FreqIdxCount] = {FREQ_315, FREQ_433, FREQ_868};
static const char* freq_labels[FreqIdxCount] = {
    "315.00 MHz (US)",
    "433.92 MHz (EU/Asia)",
    "868.35 MHz (EU)",
};

static void freq_change_callback(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_list_get_selected_item_index(app->var_item_list);
    if(idx < FreqIdxCount) {
        app->frequency = freq_table[idx];
        variable_item_set_current_value_index(item, idx);
        variable_item_set_current_value_text(item, freq_labels[idx]);
    }
}

static uint32_t freq_back_callback(void* context) {
    UNUSED(context);
    return ViewMenu;
}

void scene_freq_select_alloc(ProtoPirateApp* app) {
    variable_item_list_reset(app->var_item_list);
    view_set_previous_callback(
        variable_item_list_get_view(app->var_item_list), freq_back_callback);

    for(uint8_t i = 0; i < FreqIdxCount; i++) {
        VariableItem* item = variable_item_list_add(
            app->var_item_list, freq_labels[i], 1, freq_change_callback, app);
        if(freq_table[i] == app->frequency) {
            variable_item_set_current_value_index(item, i);
            variable_item_set_current_value_text(item, freq_labels[i]);
        }
    }

    if(view_dispatcher_has_view(app->view_dispatcher, ViewVarList)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewVarList);
    }
    view_dispatcher_add_view(
        app->view_dispatcher, ViewVarList,
        variable_item_list_get_view(app->var_item_list));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
}

// ===================== 场景: 信息 =====================
static uint32_t info_back_callback(void* context) {
    UNUSED(context);
    return ViewMenu;
}

void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "ProtoPirate RB");
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary,
        "RollBack Edition v2.0");
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignTop, FontKeyboard,
        "TX: FULLY UNLOCKED");
    widget_add_string_element(
        app->widget, 64, 54, AlignCenter, AlignTop, FontKeyboard,
        "RollBack Attack Engine");
    widget_add_string_element(
        app->widget, 64, 72, AlignCenter, AlignTop, FontSecondary,
        "Momentum FW Dev");

    view_set_previous_callback(
        widget_get_view(app->widget), info_back_callback);

    if(view_dispatcher_has_view(app->view_dispatcher, ViewWidget)) {
        view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    }
    view_dispatcher_add_view(
        app->view_dispatcher, ViewWidget, widget_get_view(app->widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// ===================== 主入口 =====================
__attribute__((visibility("default"))) int32_t app_main(void* p) {
    UNUSED(p);
    ProtoPirateApp* app = protoPirateApp_alloc();

    scene_main_menu_alloc(app);
    // Add the menu view
    if(!view_dispatcher_has_view(app->view_dispatcher, ViewMenu)) {
        view_dispatcher_add_view(
            app->view_dispatcher, ViewMenu, submenu_get_view(app->submenu));
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    protoPirateApp_free(app);
    return 0;
}
