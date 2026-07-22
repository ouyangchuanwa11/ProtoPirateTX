#include "protopirate_scene.h"

// This file defines the scene connections for the scene manager.
// Each scene has: on_enter, on_event, on_exit callbacks.

// Include scene implementations
extern void protopirate_scene_start_on_enter(void* context);
extern bool protopirate_scene_start_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_start_on_exit(void* context);

extern void protopirate_scene_receiver_on_enter(void* context);
extern bool protopirate_scene_receiver_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_receiver_on_exit(void* context);

extern void protopirate_scene_receiver_config_on_enter(void* context);
extern bool protopirate_scene_receiver_config_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_receiver_config_on_exit(void* context);

extern void protopirate_scene_receiver_info_on_enter(void* context);
extern bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_receiver_info_on_exit(void* context);

extern void protopirate_scene_emulate_on_enter(void* context);
extern bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_emulate_on_exit(void* context);

extern void protopirate_scene_saved_on_enter(void* context);
extern bool protopirate_scene_saved_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_saved_on_exit(void* context);

extern void protopirate_scene_saved_info_on_enter(void* context);
extern bool protopirate_scene_saved_info_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_saved_info_on_exit(void* context);

extern void protopirate_scene_sub_decode_on_enter(void* context);
extern bool protopirate_scene_sub_decode_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_sub_decode_on_exit(void* context);

extern void protopirate_scene_need_saving_on_enter(void* context);
extern bool protopirate_scene_need_saving_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_need_saving_on_exit(void* context);

extern void protopirate_scene_timing_tuner_on_enter(void* context);
extern bool protopirate_scene_timing_tuner_on_event(void* context, SceneManagerEvent event);
extern void protopirate_scene_timing_tuner_on_exit(void* context);

// Scene handler table
static const SceneHandlers protopirate_scene_handlers_table[] = {
    {protopirate_scene_start_on_enter, protopirate_scene_start_on_event, protopirate_scene_start_on_exit},
    {protopirate_scene_receiver_on_enter, protopirate_scene_receiver_on_event, protopirate_scene_receiver_on_exit},
    {protopirate_scene_receiver_config_on_enter, protopirate_scene_receiver_config_on_event, protopirate_scene_receiver_config_on_exit},
    {protopirate_scene_receiver_info_on_enter, protopirate_scene_receiver_info_on_event, protopirate_scene_receiver_info_on_exit},
    {protopirate_scene_emulate_on_enter, protopirate_scene_emulate_on_event, protopirate_scene_emulate_on_exit},
    {protopirate_scene_saved_on_enter, protopirate_scene_saved_on_event, protopirate_scene_saved_on_exit},
    {protopirate_scene_saved_info_on_enter, protopirate_scene_saved_info_on_event, protopirate_scene_saved_info_on_exit},
    {protopirate_scene_sub_decode_on_enter, protopirate_scene_sub_decode_on_event, protopirate_scene_sub_decode_on_exit},
    {protopirate_scene_need_saving_on_enter, protopirate_scene_need_saving_on_event, protopirate_scene_need_saving_on_exit},
    {protopirate_scene_timing_tuner_on_enter, protopirate_scene_timing_tuner_on_event, protopirate_scene_timing_tuner_on_exit},
};

const SceneManagerHandlers protopirate_scene_handlers = {
    .handlers = protopirate_scene_handlers_table,
    .handlers_count = COUNT_OF(protopirate_scene_handlers_table),
};
