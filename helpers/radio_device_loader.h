#pragma once

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

// Radio device loader
SubGhzRadioDevice* radio_device_loader_set(
    SubGhzRadioDevice* device,
    SubGhzRadioDeviceType type);

SubGhzRadioDevice* radio_device_loader_get(SubGhzRadioDeviceType type);
