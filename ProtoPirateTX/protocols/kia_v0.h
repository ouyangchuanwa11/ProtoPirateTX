#pragma once

#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>

bool kia_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
uint32_t kia_v0_build(DecodeResult* result, uint32_t* pulses, uint32_t max_pulses);
