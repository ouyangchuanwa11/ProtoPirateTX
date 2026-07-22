#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool kia_v5_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v6_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool kia_v7_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
