#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification_messages.h>
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/devices.h>

// ===================== Constants =====================
#define TAG "ProtoPirateRB"
#define PROTOPIRATE_RB_VERSION "3.1"
#define PROTOPIRATE_RB_AUTHOR "ProtoPirate RB"

// Max protocols we support
#define MAX_PROTOCOLS 32
#define MAX_BTN_ACTIONS 8

// ===================== Protocol Type Enum =====================
typedef enum {
    Proto_Kia_V0 = 0,
    Proto_Kia_V1,
    Proto_Kia_V2,
    Proto_Ford,
    Proto_Subaru,
    Proto_Fiat,
    Proto_Chrysler,
    Proto_Honda,
    Proto_Toyota,
    Proto_Starline,
    Proto_BFT,
    Proto_Unknown = 99
} ProtoType;

// ===================== Button Action =====================
typedef struct {
    uint8_t btn_id;
    char btn_name[16];
} BtnAction;

// ===================== Rollback State =====================
typedef struct {
    char proto[32];
    uint8_t protocol_type;
    uint32_t serial;
    uint8_t button;
    uint16_t base_counter;
    uint16_t target_counter;
    uint16_t current_counter;
    uint16_t step_size;
    uint8_t burst_count;
    bool running;
    uint32_t total_sent;
    bool bidirectional;
} RollbackState;

// ===================== Decode Result =====================
typedef struct {
    char proto[32];
    uint8_t bits;
    uint32_t data_hi;
    uint32_t data_lo;
    uint32_t serial;
    uint8_t button;
    char btn_name[16];
    uint32_t counter;
    bool encrypted;
} DecodeResult;

// ===================== Batch State =====================
typedef struct {
    uint32_t count;
    uint32_t sent_so_far;
    bool active;
} BatchState;

// ===================== Protocol Info =====================
typedef struct {
    char proto[32];
    uint8_t proto_type;
    uint32_t freq;
    uint32_t sample_count;
} ProtoInfo;

// ===================== App Scenes =====================
typedef enum {
    ProtoPirateSceneMainMenu,
    ProtoPirateSceneDecode,
    ProtoPirateSceneDecodeResult,
    ProtoPirateSceneRollbackConfig,
    ProtoPirateSceneRollbackRun,
    ProtoPirateSceneBatchConfig,
    ProtoPirateSceneBatchRun,
    ProtoPirateSceneTXRaw,
    ProtoPirateSceneTXCustom,
    ProtoPirateSceneAbout,
    ProtoPirateSceneCount
} ProtoPirateScene;

typedef enum {
    ProtoPirateViewSubmenu,
    ProtoPirateViewWidget,
    ProtoPirateViewTextInput,
    ProtoPirateViewVariableItemList,
    ProtoPirateViewDialog,
} ProtoPirateView;

// ===================== Main App =====================
typedef struct {
    // Core
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    // Views
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    DialogEx* dialog;

    // TX
    uint32_t tx_data_hi;
    uint32_t tx_data_lo;
    uint32_t tx_freq;
    uint8_t tx_repeats;
    bool tx_busy;

    // Frequency
    uint32_t frequency;

    // Current decode result
    DecodeResult last_result;

    // Captured raw signal
    FuriString* captured_signal;

    // Rollback state
    RollbackState rollback;

    // Batch state
    BatchState batch;

    // Protocol list
    ProtoInfo protocols[MAX_PROTOCOLS];
    uint8_t protocol_count;

    // UI strings
    FuriString* info_str;

    // Current scene tracking for callbacks
    uint32_t current_scene;
} ProtoPirateApp;

// ===================== Decoder Functions =====================
DecodeResult* decode_signal(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_kia_v0(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_kia_v1(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_kia_v2(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_ford(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_subaru(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_fiat(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_chrysler(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_honda(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_toyota(ProtoPirateApp* app, FuriString* raw_data);
DecodeResult* decode_starline(ProtoPirateApp* app, FuriString* raw_data);
uint8_t kia_crc8(uint8_t* data, uint8_t len);
const char* get_button_name(const char* proto, uint8_t btn);

// ===================== TX Functions =====================
bool tx_device_init(void);
void tx_device_deinit(void);
bool tx_init_hw(ProtoPirateApp* app, uint32_t freq);
bool transmit_packet(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep);
void transmit_packet_nonblock(ProtoPirateApp* app, uint32_t dhi, uint32_t dlo, uint32_t freq, uint8_t rep);
void transmit_packet_wait(ProtoPirateApp* app);
void transmit_packet_stop(ProtoPirateApp* app);
void transmit_start(ProtoPirateApp* app, uint32_t freq);
void transmit_burst(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo);
void transmit_stop(ProtoPirateApp* app);
bool transmit_raw(ProtoPirateApp* app, FuriString* raw_data, uint32_t freq, uint8_t repeats);

// ===================== Rollback Functions =====================
void rollback_build_frame(uint32_t serial, uint8_t button, uint32_t counter, uint32_t* data_hi, uint32_t* data_lo);
void rollback_build_frame_proto(uint8_t proto_type, uint32_t serial, uint8_t button, uint32_t counter, uint32_t* data_hi, uint32_t* data_lo);
bool rollback_send_single(ProtoPirateApp* app, uint32_t serial, uint8_t button, uint16_t counter);
bool rollback_attack_run(ProtoPirateApp* app);
bool rollback_bidirectional_attack(ProtoPirateApp* app);
uint8_t rollback_crc8_compute(uint8_t* data, uint8_t len);
uint8_t rollback_get_button_value(uint8_t proto_type, uint8_t btn_idx);

// ===================== Scene Handlers =====================
void protopirate_rb_app(void* p);
void protopirate_scene_main_menu_on_enter(void* context);
bool protopirate_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_main_menu_on_exit(void* context);
void protopirate_scene_decode_on_enter(void* context);
bool protopirate_scene_decode_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_decode_on_exit(void* context);
void protopirate_scene_decode_result_on_enter(void* context);
bool protopirate_scene_decode_result_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_decode_result_on_exit(void* context);
void protopirate_scene_rollback_config_on_enter(void* context);
bool protopirate_scene_rollback_config_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_rollback_config_on_exit(void* context);
void protopirate_scene_rollback_run_on_enter(void* context);
bool protopirate_scene_rollback_run_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_rollback_run_on_exit(void* context);
void protopirate_scene_batch_config_on_enter(void* context);
bool protopirate_scene_batch_config_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_batch_config_on_exit(void* context);
void protopirate_scene_batch_run_on_enter(void* context);
bool protopirate_scene_batch_run_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_batch_run_on_exit(void* context);
void protopirate_scene_tx_raw_on_enter(void* context);
bool protopirate_scene_tx_raw_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_tx_raw_on_exit(void* context);
void protopirate_scene_tx_custom_on_enter(void* context);
bool protopirate_scene_tx_custom_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_tx_custom_on_exit(void* context);
void protopirate_scene_about_on_enter(void* context);
bool protopirate_scene_about_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_about_on_exit(void* context);

// ===================== Batch Functions =====================
void batch_send_start(ProtoPirateApp* app);
void batch_send_stop(ProtoPirateApp* app);

// ===================== External device ref =====================
extern const SubGhzDevice* g_device;