#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool subaru_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
