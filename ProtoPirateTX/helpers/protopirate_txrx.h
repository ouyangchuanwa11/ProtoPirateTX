#pragma once

#include "../protopirate_app_i.h"

// TX/RX management
bool protopirate_rx_start(ProtoPirateApp* app);
void protopirate_rx_stop(ProtoPirateApp* app);
bool protopirate_tx_start(ProtoPirateApp* app, uint32_t frequency);
void protopirate_tx_stop(ProtoPirateApp* app);
void protopirate_tx_data(ProtoPirateApp* app, uint32_t* pulses, uint32_t num_pulses, uint32_t frequency);
bool protopirate_process_received_data(ProtoPirateApp* app);
void protopirate_rx_callback(uint32_t* pulses, uint32_t num_pulses, void* context);
