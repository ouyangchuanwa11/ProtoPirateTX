#pragma once

#include "defines.h"
#include "protocols/protocol_items.h"
#include "helpers/protopirate_types.h"
#include "helpers/protopirate_txrx.h"
#include "helpers/protopirate_storage.h"
#include "helpers/protopirate_radio.h"
#include "helpers/protopirate_settings.h"
#include "helpers/protopirate_views.h"
#include "helpers/protopirate_saved_match.h"
#include "helpers/radio_device_loader.h"
#include "helpers/raw_file_reader.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOPIRATE_VERSION "2.0"

typedef struct ProtoPirateApp ProtoPirateApp;

// Forward declare scene handlers
typedef enum {
    ProtoPirateSceneStart,
    ProtoPirateSceneReceiver,
    ProtoPirateSceneReceiverConfig,
    ProtoPirateSceneReceiverInfo,
    ProtoPirateSceneEmulate,
    ProtoPirateSceneSaved,
    ProtoPirateSceneSavedInfo,
    ProtoPirateSceneSubDecode,
    ProtoPirateSceneNeedSaving,
    ProtoPirateSceneTimingTuner,
    ProtoPirateSceneCount,
} ProtoPirateScene;

// Saved signal entry
typedef struct {
    char name[64];
    DecodeResult result;
    bool has_raw_data;
} SavedSignal;

// History entry
typedef struct {
    DecodeResult result;
    FuriString* raw_data_str;
    bool saved;
} HistoryEntry;

// History collection
typedef struct {
    HistoryEntry* entries[PROTOPIRATE_HISTORY_MAX];
    uint32_t count;
    uint32_t index; // current selected
} ProtoPirateHistory;

// Main application structure
struct ProtoPirateApp {
    // Core Flipper
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    // UI modules
    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    TextInput* text_input;
    VariableItemList* variable_item_list;

    // Custom views
    View* receiver_view;
    View* emulate_view;

    // Sub-GHz
    SubGhzTxRxWorker* txrx_worker;
    SubGhzRadioDevice* radio_device;
    bool tx_active;
    bool rx_active;

    // Decode state
    DecodeState* decode_state;
    DecodeResult last_result;

    // History
    ProtoPirateHistory* history;

    // Saved signals
    SavedSignal saved_signals[100];
    uint32_t saved_count;

    // Settings
    ProtoPirateSettings settings;

    // File operations
    FuriString* file_path;
    bool file_loaded;

    // State
    bool need_saving;
    uint32_t current_frequency;
    FrequencyBand frequency_band;
    uint32_t selected_protocol_index;

    // Counter rollback
    int32_t rollback_amount;
    bool rollback_enabled;

    // Timing tuner state
    float timing_factor;
};

// ProtoPirateHistory functions
ProtoPirateHistory* protopirate_history_alloc(void);
void protopirate_history_free(ProtoPirateHistory* history);
void protopirate_history_add(ProtoPirateHistory* history, DecodeResult* result);
void protopirate_history_clear(ProtoPirateHistory* history);
HistoryEntry* protopirate_history_get(ProtoPirateHistory* history, uint32_t index);
uint32_t protopirate_history_get_count(ProtoPirateHistory* history);

// Scene callbacks
void protopirate_scene_start_on_enter(void* context);
bool protopirate_scene_start_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_start_on_exit(void* context);

void protopirate_scene_receiver_on_enter(void* context);
bool protopirate_scene_receiver_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_receiver_on_exit(void* context);

void protopirate_scene_receiver_config_on_enter(void* context);
bool protopirate_scene_receiver_config_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_receiver_config_on_exit(void* context);

void protopirate_scene_receiver_info_on_enter(void* context);
bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_receiver_info_on_exit(void* context);

void protopirate_scene_emulate_on_enter(void* context);
bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_emulate_on_exit(void* context);

void protopirate_scene_saved_on_enter(void* context);
bool protopirate_scene_saved_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_saved_on_exit(void* context);

void protopirate_scene_saved_info_on_enter(void* context);
bool protopirate_scene_saved_info_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_saved_info_on_exit(void* context);

void protopirate_scene_sub_decode_on_enter(void* context);
bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_sub_decode_on_exit(void* context);

void protopirate_scene_need_saving_on_enter(void* context);
bool protopirate_scene_need_saving_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_need_saving_on_exit(void* context);

void protopirate_scene_timing_tuner_on_enter(void* context);
bool protopirate_scene_timing_tuner_on_event(void* context, SceneManagerEvent event);
void protopirate_scene_timing_tuner_on_exit(void* context);

#ifdef __cplusplus
}
#endif
