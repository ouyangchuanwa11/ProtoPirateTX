#pragma once

#include "../protopirate_app_i.h"

// Storage management
void protopirate_storage_save_signal(ProtoPirateApp* app);
void protopirate_storage_save_all(ProtoPirateApp* app);
void protopirate_storage_load_saved(ProtoPirateApp* app);
bool protopirate_storage_delete_signal(ProtoPirateApp* app, uint32_t index);
void protopirate_storage_ensure_dir(void);
