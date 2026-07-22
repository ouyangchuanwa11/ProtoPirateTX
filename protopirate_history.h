#pragma once

#include <furi.h>
#include "protopirate_app_i.h"

ProtoPirateHistory* protopirate_history_alloc(void);
void protopirate_history_free(ProtoPirateHistory* history);
void protopirate_history_add(ProtoPirateHistory* history, DecodeResult* result);
void protopirate_history_clear(ProtoPirateHistory* history);
HistoryEntry* protopirate_history_get(ProtoPirateHistory* history, uint32_t index);
uint32_t protopirate_history_get_count(ProtoPirateHistory* history);
