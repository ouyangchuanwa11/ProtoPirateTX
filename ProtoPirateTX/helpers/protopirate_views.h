#pragma once

#include "../protopirate_app_i.h"
#include <gui/view.h>

// Create/destroy shared views
View* protopirate_view_receiver_alloc(ProtoPirateApp* app);
View* protopirate_view_emulate_alloc(ProtoPirateApp* app);
void protopirate_view_receiver_free(View* view);
void protopirate_view_emulate_free(View* view);

// Update functions
void protopirate_view_receiver_update(View* view, DecodeState* state);
void protopirate_view_emulate_update(View* view, DecodeResult* result);
