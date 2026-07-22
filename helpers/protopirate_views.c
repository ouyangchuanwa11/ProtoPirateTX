#include "protopirate_views.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view.h>

// Receiver view draw callback
static void protopirate_view_receiver_draw(Canvas* canvas, void* model) {
    furi_assert(canvas);
    furi_assert(model);

    DecodeState* state = (DecodeState*)model;
    char buf[64];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(state && state->decoded && state->result.valid) {
        // Draw decoded signal info
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop,
                                state->result.protocol ? state->result.protocol->display_name : "???");

        canvas_set_font(canvas, FontSecondary);

        snprintf(buf, sizeof(buf), "S/N: %08lX", state->result.serial);
        canvas_draw_str(canvas, 5, 20, buf);

        snprintf(buf, sizeof(buf), "Btn: %lu", state->result.button);
        canvas_draw_str(canvas, 5, 32, buf);

        snprintf(buf, sizeof(buf), "Cnt: %lu", state->result.counter);
        canvas_draw_str(canvas, 5, 44, buf);

        snprintf(buf, sizeof(buf), "CRC: %04lX", state->result.crc);
        canvas_draw_str(canvas, 5, 56, buf);

        snprintf(buf, sizeof(buf), "Freq: %lu Hz", state->result.frequency);
        canvas_draw_str(canvas, 5, 68, buf);

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 64, 122, AlignCenter, AlignBottom,
                                "OK:TX  BACK:Menu");
    } else if(state && state->num_pulses > 0) {
        // Receiving pulses
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Receiving...");

        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Pulses: %lu", state->num_pulses);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, buf);

        // Show signal activity
        uint32_t width = MIN(state->num_pulses / 5, 120);
        for(uint32_t i = 0; i < width && i < 120; i++) {
            uint32_t pulse_val = state->pulses[i * 5] / 50;
            if(pulse_val > 30) pulse_val = 30;
            canvas_draw_dot(canvas, 4 + i, 90 - pulse_val);
        }
    } else {
        // Waiting for signal
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Waiting for signal...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter,
                                "Point remote at Flipper");
    }
}

// Allocate receiver view
View* protopirate_view_receiver_alloc(ProtoPirateApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, protopirate_view_receiver_draw);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DecodeState));
    return view;
}

// Free receiver view
void protopirate_view_receiver_free(View* view) {
    if(view) {
        view_free(view);
    }
}

// Update receiver view model
void protopirate_view_receiver_update(View* view, DecodeState* state) {
    furi_assert(view);
    furi_assert(state);

    DecodeState* model = view_get_model(view);
    if(model) {
        memcpy(model, state, sizeof(DecodeState));
        view_commit_model(view, true);
    }
}

// Emulate view draw callback
static void protopirate_view_emulate_draw(Canvas* canvas, void* model) {
    furi_assert(canvas);
    furi_assert(model);

    DecodeResult* result = (DecodeResult*)model;
    char buf[64];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(!result || !result->valid) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No signal");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "TX Mode");

    canvas_set_font(canvas, FontSecondary);

    snprintf(buf, sizeof(buf), "Protocol: %s",
             result->protocol ? result->protocol->display_name : "Unknown");
    canvas_draw_str(canvas, 5, 20, buf);

    snprintf(buf, sizeof(buf), "Serial: %08lX", result->serial);
    canvas_draw_str(canvas, 5, 32, buf);

    snprintf(buf, sizeof(buf), "Button: %lu  Counter: %lu", result->button, result->counter);
    canvas_draw_str(canvas, 5, 44, buf);

    snprintf(buf, sizeof(buf), "CRC: %04lX", result->crc);
    canvas_draw_str(canvas, 5, 56, buf);

    if(result->protocol && (result->protocol->features & PROTO_HAS_ROLLBACK)) {
        canvas_draw_str(canvas, 5, 70, "<- Rollback   -> TX");
    } else {
        canvas_draw_str(canvas, 5, 70, "OK = Transmit");
    }
}

// Allocate emulate view
View* protopirate_view_emulate_alloc(ProtoPirateApp* app) {
    View* view = view_alloc();
    view_set_draw_callback(view, protopirate_view_emulate_draw);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DecodeResult));
    return view;
}

// Free emulate view
void protopirate_view_emulate_free(View* view) {
    if(view) {
        view_free(view);
    }
}

// Update emulate view model
void protopirate_view_emulate_update(View* view, DecodeResult* result) {
    furi_assert(view);
    furi_assert(result);

    DecodeResult* model = view_get_model(view);
    if(model) {
        memcpy(model, result, sizeof(DecodeResult));
        view_commit_model(view, true);
    }
}
