#pragma once

#include <gui/scene_manager.h>
#include "../protopirate_app_i.h"

// Scene handler storage
extern const SceneManagerHandlers protopirate_scene_handlers;

// Scene on_enter/on_event/on_exit callbacks
// Defined in individual scene files

// Auto-generated scene IDs from scene config
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
