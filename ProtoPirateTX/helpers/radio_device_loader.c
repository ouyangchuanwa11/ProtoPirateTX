#include "radio_device_loader.h"
#include "../defines.h"

#include <furi.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

SubGhzRadioDevice* radio_device_loader_set(
    SubGhzRadioDevice* device,
    SubGhzRadioDeviceType type) {
    UNUSED(device);

    switch(type) {
    case SubGhzRadioDeviceTypeInternal:
        return subghz_devices_get_by_name("cc1101_int");
    case SubGhzRadioDeviceTypeExternalCC1101:
        return subghz_devices_get_by_name("cc1101_ext");
    default:
        return NULL;
    }
}

SubGhzRadioDevice* radio_device_loader_get(SubGhzRadioDeviceType type) {
    return radio_device_loader_set(NULL, type);
}
