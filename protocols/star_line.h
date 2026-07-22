#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool star_line_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool scher_khan_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool mitsubishi_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool mazda_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
