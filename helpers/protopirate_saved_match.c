#include "protopirate_saved_match.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>

// Check if a received signal matches any saved signal (by serial + protocol)
bool protopirate_saved_match_find(ProtoPirateApp* app, DecodeResult* result) {
    furi_assert(app);
    furi_assert(result);

    if(!result->valid) return false;

    for(uint32_t i = 0; i < app->saved_count; i++) {
        SavedSignal* saved = &app->saved_signals[i];

        if(saved->result.serial == result->serial &&
           saved->result.protocol == result->protocol) {
            return true;
        }
    }

    return false;
}

// Find the index of a matching saved signal, or -1
int32_t protopirate_saved_match_index(ProtoPirateApp* app, DecodeResult* result) {
    furi_assert(app);
    furi_assert(result);

    if(!result->valid) return -1;

    for(uint32_t i = 0; i < app->saved_count; i++) {
        SavedSignal* saved = &app->saved_signals[i];

        if(saved->result.serial == result->serial &&
           saved->result.protocol == result->protocol) {
            return (int32_t)i;
        }
    }

    return -1;
}
