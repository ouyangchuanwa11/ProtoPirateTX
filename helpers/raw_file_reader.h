#pragma once

#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

// RAW file reader - decodes .sub files
bool protopirate_raw_file_decode(FuriString* file_path, DecodeResult* result);
bool protopirate_raw_file_load_pulses(FuriString* file_path, uint32_t* pulses, uint32_t* num_pulses);
