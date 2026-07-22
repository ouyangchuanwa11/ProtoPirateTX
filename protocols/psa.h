#pragma once
#include "../defines.h"
#include "../helpers/protopirate_types.h"
#include <furi.h>
#include <stdbool.h>
bool psa_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool porsche_touareg_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool renault_v0_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool vag_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
bool aut64_decode(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result);
