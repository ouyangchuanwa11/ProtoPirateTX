#pragma once

#include "protopirate_scene.h"

// Scene navigation - defines custom transitions
// Usage: add event to go to a specific scene

// Scene to scene transitions config
typedef struct {
    ProtoPirateScene from;
    uint32_t event_type; // SceneManagerEventTypeCustom
    ProtoPirateScene to;
} ProtoPirateSceneTransition;

// Common custom events for scene navigation
enum ProtoPirateCustomEvent {
    // Start menu events
    ProtoPirateCustomEventStartReceive = 100,
    ProtoPirateCustomEventStartSaved,
    ProtoPirateCustomEventStartSubDecode,
    ProtoPirateCustomEventStartAbout,
    ProtoPirateCustomEventStartConfig,
    ProtoPirateCustomEventStartTimingTuner,

    // Receiver events
    ProtoPirateCustomEventReceiverNewData,
    ProtoPirateCustomEventReceiverOk,
    ProtoPirateCustomEventReceiverConfig,

    // Emulate events
    ProtoPirateCustomEventEmulateTX,
    ProtoPirateCustomEventEmulateRollback0,
    ProtoPirateCustomEventEmulateRollback1,
    ProtoPirateCustomEventEmulateRollback10,
    ProtoPirateCustomEventEmulateRollback100,
    ProtoPirateCustomEventEmulateCancel,
    ProtoPirateCustomEventEmulateSave,

    // Saved events
    ProtoPirateCustomEventSavedSelect,
    ProtoPirateCustomEventSavedDelete,

    // Need saving events
    ProtoPirateCustomEventNeedSavingYes,
    ProtoPirateCustomEventNeedSavingNo,
    ProtoPirateCustomEventNeedSavingCancel,

    // Timing tuner events
    ProtoPirateCustomEventTimingIncrease,
    ProtoPirateCustomEventTimingDecrease,
    ProtoPirateCustomEventTimingReset,
};
