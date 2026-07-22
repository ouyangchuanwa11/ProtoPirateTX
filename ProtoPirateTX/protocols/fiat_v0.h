#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool fiat_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool fiat_v1_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool fiat_v2_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
