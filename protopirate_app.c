#include "protopirate_app_i.h"
#include "scenes/protopirate_scene.h"
#include <furi.h>
#include <furi_hal.h>
#include <notification/notification_messages.h>

static const uint32_t frequency_list_300[] = {
    300000000,
    303875000,
    304250000,
    310000000,
    315000000,
    318000000,
    320000000,
    330000000,
    345000000,
    348000000,
};

static const uint32_t frequency_list_387[] = {
    387000000,
    390000000,
    418000000,
    433075000,
    433220000,
    433420000,
    433920000,
    434420000,
    434775000,
    434790000,
    438900000,
    464000000,
};

static const uint32_t frequency_list_779[] = {
    779000000,
    868350000,
    868400000,
    868700000,
    868800000,
    868950000,
    906400000,
    915000000,
    925000000,
    928000000,
};

static const uint32_t* const frequency_lists[] = {
    [FrequencyBand300] = frequency_list_300,
    [FrequencyBand387] = frequency_list_387,
    [FrequencyBand779] = frequency_list_779,
};

static const uint32_t frequency_list_counts[] = {
    [FrequencyBand300] = COUNT_OF(frequency_list_300),
    [FrequencyBand387] = COUNT_OF(frequency_list_387),
    [FrequencyBand779] = COUNT_OF(frequency_list_779),
};

// View draw callbacks
static void protopirate_receiver_view_draw_callback(Canvas* canvas, void* model) {
    furi_assert(canvas);
    furi_assert(model);

    DecodeState* state = (DecodeState*)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(state && state->decoded && state->result.valid) {
        // Draw decoded info
        char buf[64];
        canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, state->result.protocol->display_name);

        canvas_set_font(canvas, FontSecondary);

        snprintf(buf, sizeof(buf), "Serial: %08lX", state->result.serial);
        canvas_draw_str(canvas, 5, 25, buf);

        snprintf(buf, sizeof(buf), "Btn: %lu  Cnt: %lu", state->result.button, state->result.counter);
        canvas_draw_str(canvas, 5, 37, buf);

        snprintf(buf, sizeof(buf), "CRC: %04lX", state->result.crc);
        canvas_draw_str(canvas, 5, 49, buf);

        if(state->result.protocol->features & PROTO_HAS_FIXED) {
            snprintf(buf, sizeof(buf), "Fixed: %08lX", state->result.fixed_code);
            canvas_draw_str(canvas, 5, 61, buf);
        }

        canvas_draw_str(canvas, 5, 73, "Press OK to TX/Emulate");
    } else if(state && state->num_pulses > 0) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Receiving...");
        canvas_set_font(canvas, FontSecondary);

        snprintf(buf, sizeof(buf), "Pulses: %lu", state->num_pulses);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, buf);

        snprintf(buf, sizeof(buf), "Freq: %lu Hz", protopirate_app->current_frequency);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignCenter, buf);
    } else {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Waiting for signal...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Point remote at Flipper");
    }
}

static void protopirate_emulate_view_draw_callback(Canvas* canvas, void* model) {
    furi_assert(canvas);
    furi_assert(model);

    DecodeResult* result = (DecodeResult*)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(!result || !result->valid) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No signal selected");
        return;
    }

    char buf[64];
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "TX Ready");

    canvas_set_font(canvas, FontSecondary);

    snprintf(buf, sizeof(buf), "Protocol: %s", result->protocol->display_name);
    canvas_draw_str(canvas, 5, 22, buf);

    snprintf(buf, sizeof(buf), "Serial: %08lX", result->serial);
    canvas_draw_str(canvas, 5, 34, buf);

    snprintf(buf, sizeof(buf), "Button: %lu   Counter: %lu", result->button, result->counter);
    canvas_draw_str(canvas, 5, 46, buf);

    snprintf(buf, sizeof(buf), "CRC: %04lX", result->crc);
    canvas_draw_str(canvas, 5, 58, buf);

    if(result->protocol->features & PROTO_HAS_ROLLBACK) {
        canvas_draw_str(canvas, 5, 70, "<- Rollback   -> TX");
    } else {
        canvas_draw_str(canvas, 5, 70, "OK: Transmit");
    }
}

// App entry point
int32_t protopirate_app(void* p) {
    UNUSED(p);

    // Allocate app
    ProtoPirateApp* app = malloc(sizeof(ProtoPirateApp));
    memset(app, 0, sizeof(ProtoPirateApp));

    // Initialize core
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&protopirate_scene_handlers, app);

    // Submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdWidget, widget_get_view(app->widget));

    // Popup
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdPopup, popup_get_view(app->popup));

    // Text input
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    // Variable item list
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdVariableItemList, variable_item_list_get_view(app->variable_item_list));

    // Custom views
    // Receiver view
    app->receiver_view = view_alloc();
    view_set_draw_callback(app->receiver_view, protopirate_receiver_view_draw_callback);
    view_set_context(app->receiver_view, app);
    view_allocate_model(app->receiver_view, ViewModelTypeLocking, sizeof(DecodeState));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdReceiver, app->receiver_view);

    // Emulate view
    app->emulate_view = view_alloc();
    view_set_draw_callback(app->emulate_view, protopirate_emulate_view_draw_callback);
    view_set_context(app->emulate_view, app);
    view_allocate_model(app->emulate_view, ViewModelTypeLocking, sizeof(DecodeResult));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdEmulate, app->emulate_view);

    // Init modules
    app->history = protopirate_history_alloc();
    app->decode_state = malloc(sizeof(DecodeState));
    memset(app->decode_state, 0, sizeof(DecodeState));
    app->decode_state->pulses = malloc(sizeof(uint32_t) * PROTOPIRATE_MAX_PULSES);
    app->decode_state->num_pulses = 0;

    app->file_path = furi_string_alloc();
    app->current_frequency = PROTOPIRATE_DEFAULT_FREQ;
    app->frequency_band = FrequencyBand387;
    app->timing_factor = 1.0f;
    app->tx_active = false;
    app->rx_active = false;
    app->txrx_worker = NULL;
    app->radio_device = NULL;
    app->need_saving = false;

    // Load saved signals
    protopirate_storage_load_saved(app);

    // Load settings
    protopirate_settings_load(&app->settings);

    // Initialize protocols
    protopirate_protocols_init();

    // Set default frequency
    app->current_frequency = app->settings.last_frequency ? app->settings.last_frequency : PROTOPIRATE_DEFAULT_FREQ;

    // Start at main menu
    scene_manager_next_scene(app->scene_manager, ProtoPirateSceneStart);

    // Run dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    if(app->rx_active) {
        protopirate_rx_stop(app);
    }
    if(app->tx_active) {
        protopirate_tx_stop(app);
    }

    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdReceiver);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdEmulate);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdVariableItemList);

    // Free modules
    view_free(app->receiver_view);
    view_free(app->emulate_view);
    submenu_free(app->submenu);
    widget_free(app->widget);
    popup_free(app->popup);
    text_input_free(app->text_input);
    variable_item_list_free(app->variable_item_list);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    protopirate_history_free(app->history);
    free(app->decode_state->pulses);
    free(app->decode_state);
    furi_string_free(app->file_path);

    if(app->radio_device) {
        subghz_devices_reset(app->radio_device);
    }

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);

    return 0;
}
