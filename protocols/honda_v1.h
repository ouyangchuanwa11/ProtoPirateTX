#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool honda_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool honda_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool honda_static_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
