#include "protopirate_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <lib/toolbox/path.h>

ProtoPirateHistory* protopirate_history_alloc(void) {
    ProtoPirateHistory* history = malloc(sizeof(ProtoPirateHistory));
    memset(history, 0, sizeof(ProtoPirateHistory));
    return history;
}

void protopirate_history_free(ProtoPirateHistory* history) {
    furi_assert(history);
    protopirate_history_clear(history);
    free(history);
}

void protopirate_history_add(ProtoPirateHistory* history, DecodeResult* result) {
    furi_assert(history);
    furi_assert(result);

    if(history->count >= PROTOPIRATE_HISTORY_MAX) {
        // Remove oldest
        HistoryEntry* oldest = history->entries[0];
        if(oldest) {
            if(oldest->raw_data_str) furi_string_free(oldest->raw_data_str);
            if(oldest->result.raw_data) free(oldest->result.raw_data);
            free(oldest);
        }
        memmove(&history->entries[0], &history->entries[1], (PROTOPIRATE_HISTORY_MAX - 1) * sizeof(HistoryEntry*));
        history->count--;
    }

    HistoryEntry* entry = malloc(sizeof(HistoryEntry));
    memset(entry, 0, sizeof(HistoryEntry));
    memcpy(&entry->result, result, sizeof(DecodeResult));

    // Copy raw data if present
    if(result->raw_data && result->raw_len > 0) {
        entry->result.raw_data = malloc(result->raw_len);
        memcpy(entry->result.raw_data, result->raw_data, result->raw_len);
        entry->result.raw_len = result->raw_len;
    }

    entry->raw_data_str = furi_string_alloc();
    entry->saved = false;

    history->entries[history->count] = entry;
    history->index = history->count;
    history->count++;
}

void protopirate_history_clear(ProtoPirateHistory* history) {
    furi_assert(history);
    for(uint32_t i = 0; i < history->count; i++) {
        if(history->entries[i]) {
            if(history->entries[i]->raw_data_str)
                furi_string_free(history->entries[i]->raw_data_str);
            if(history->entries[i]->result.raw_data)
                free(history->entries[i]->result.raw_data);
            free(history->entries[i]);
            history->entries[i] = NULL;
        }
    }
    history->count = 0;
    history->index = 0;
}

HistoryEntry* protopirate_history_get(ProtoPirateHistory* history, uint32_t index) {
    furi_assert(history);
    if(index < history->count) {
        return history->entries[index];
    }
    return NULL;
}

uint32_t protopirate_history_get_count(ProtoPirateHistory* history) {
    furi_assert(history);
    return history->count;
}
