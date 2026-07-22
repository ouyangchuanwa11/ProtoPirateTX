#pragma once

#include "../protopirate_app_i.h"

// Radio device operations
SubGhzRadioDevice* protopirate_radio_init(uint32_t frequency);
void protopirate_radio_deinit(SubGhzRadioDevice* device);
bool protopirate_radio_set_frequency(SubGhzRadioDevice* device, uint32_t frequency);
bool protopirate_radio_start_rx(SubGhzRadioDevice* device);
void protopirate_radio_stop_rx(SubGhzRadioDevice* device);
bool protopirate_radio_start_tx(SubGhzRadioDevice* device);
void protopirate_radio_stop_tx(SubGhzRadioDevice* device);
void protopirate_radio_send_raw(SubGhzRadioDevice* device, uint32_t* pulses, uint32_t num_pulses);
