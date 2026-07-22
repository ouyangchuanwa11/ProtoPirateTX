#pragma once

#include "../protopirate_app_i.h"

// Match saved signals against received data
bool protopirate_saved_match_find(ProtoPirateApp* app, DecodeResult* result);
int32_t protopirate_saved_match_index(ProtoPirateApp* app, DecodeResult* result);
