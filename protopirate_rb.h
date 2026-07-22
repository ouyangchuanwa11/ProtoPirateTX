#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/button_menu.h>
#include <input/input.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/protocol_items.h>
#include <furi_hal_subghz.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <toolbox/path.h>
#include <flipper_format/flipper_format.h>

#define TAG "ProtoPirateRB"
#define MAX_HISTORY 30
#define FREQ_315   315000000
#define FREQ_433   433920000
#define FREQ_868   868350000
#define DEFAULT_FREQ FREQ_433
#define ROLLBACK_BURST_DEFAULT 3

// ============ ???? ============
typedef struct {
    char proto[20];
    uint8_t bits;
    uint32_t data_hi;
    uint32_t data_lo;
    uint32_t serial;
    uint8_t button;
    char btn_name[16];
    uint16_t counter;
    bool crc_ok;
    bool encrypted;
} DecodeResult;

// ============ ???? ============
typedef struct {
    DecodeResult result;
    FuriString* raw_data;
    uint32_t frequency;
    char preset[20];
} HistoryItem;

// ============ RollBack?? ============
typedef struct {
    bool running;
    uint16_t base_counter;
    uint16_t target_counter;
    uint16_t current_counter;
    uint16_t step_size;
    uint8_t button;
    uint32_t serial;
    char proto[20];
    uint32_t data_hi;
    uint32_t data_lo;
    bool found;
    uint8_t burst_count;
} RollbackState;

// ============ ????? ============
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* var_item_list;
    TextBox* text_box;
    Widget* widget;
    ButtonMenu* button_menu;
    Popup* popup;

    SubGhzWorker* subghz_worker;
    SubGhzTxRxWorker* txrx_worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzTransmitter* transmitter;
    SubGhzProtocolDecoderBase* decoder_result;
    FlipperFormat* fff_data;

    uint32_t frequency;
    HistoryItem history[MAX_HISTORY];
    uint8_t history_count;
    DecodeResult last_result;
    FuriString* last_raw;
    RollbackState rollback;

    uint32_t scene;
    uint32_t submenu_index;
    uint32_t result_menu_index;
} ProtoPirateApp;

// ============ ???? ============
typedef enum {
    SceneMainMenu,
    SceneReceive,
    SceneResult,
    SceneSubLoad,
    SceneHistory,
    SceneRollback,
    SceneRollbackSingle,
    SceneRollbackSettings,
    SceneReplay,
    SceneBruteForce,
    SceneFreqSelect,
    SceneInfo,
} AppScene;

// ============ ???? ============
ProtoPirateApp* protoPirateApp_alloc();
void protoPirateApp_free(ProtoPirateApp* app);
int32_t protopirate_rb_app_entry(void* p);

// ??
void scene_main_menu_alloc(ProtoPirateApp* app);
void scene_main_menu_callback(void* context, uint32_t index);

// ??
void scene_receive_alloc(ProtoPirateApp* app);
void scene_receive_enter(ProtoPirateApp* app);

// ??
void scene_result_alloc(ProtoPirateApp* app);
void scene_result_enter(ProtoPirateApp* app);

// ?Ghz????
void scene_sub_load_alloc(ProtoPirateApp* app);
void scene_sub_load_enter(ProtoPirateApp* app);

// ??
void scene_history_alloc(ProtoPirateApp* app);

// RollBack
void scene_rollback_alloc(ProtoPirateApp* app);
void scene_rollback_enter(ProtoPirateApp* app);
void scene_rollback_single_alloc(ProtoPirateApp* app);
void scene_rollback_settings_alloc(ProtoPirateApp* app);

// ??
void scene_replay_alloc(ProtoPirateApp* app);

// ????
void scene_freq_select_alloc(ProtoPirateApp* app);
void scene_freq_select_enter(ProtoPirateApp* app);

// ??
void scene_info_alloc(ProtoPirateApp* app);

// ???
DecodeResult* decode_signal(ProtoPirateApp* app, FuriString* raw_data);

// TX(??????)
bool transmit_raw(ProtoPirateApp* app, FuriString* raw_data, uint32_t freq, uint8_t repeats);
bool transmit_packet(ProtoPirateApp* app, uint32_t data_hi, uint32_t data_lo, uint32_t freq, uint8_t repeats);

// RollBack??
bool rollback_attack_run(ProtoPirateApp* app);
bool rollback_send_single(ProtoPirateApp* app, uint32_t serial, uint8_t button, uint16_t counter);
void rollback_build_frame(uint32_t serial, uint8_t button, uint16_t counter, uint32_t* data_hi, uint32_t* data_lo);

// CRC
uint8_t kia_crc8(uint8_t* data, uint8_t len);

// ????
const char* get_button_name(const char* proto, uint8_t btn);
