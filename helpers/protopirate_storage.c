#include "protopirate_storage.h"
#include "../protopirate_app_i.h"
#include "../defines.h"

#include <furi.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <flipper_format/flipper_format.h>

#define PROTOPIRATE_SAVE_DIR "/ext/apps_data/protopirate"
#define PROTOPIRATE_SAVE_FILE "/ext/apps_data/protopirate/signals.txt"
#define PROTOPIRATE_SETTINGS_FILE "/ext/apps_data/protopirate/settings.txt"

void protopirate_storage_ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PROTOPIRATE_SAVE_DIR);
    furi_record_close(RECORD_STORAGE);
}

void protopirate_storage_save_signal(ProtoPirateApp* app) {
    furi_assert(app);
    if(!app->last_result.valid) return;

    protopirate_storage_ensure_dir();
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Find first empty slot or append
    uint32_t index = app->saved_count;
    if(index >= 100) return;

    // Create a name
    snprintf(app->saved_signals[index].name, sizeof(app->saved_signals[index].name),
             "%s_%08lX",
             app->last_result.protocol ? app->last_result.protocol->display_name : "UNKNOWN",
             app->last_result.serial);

    memcpy(&app->saved_signals[index].result, &app->last_result, sizeof(DecodeResult));
    app->saved_signals[index].has_raw_data = true;
    app->saved_count++;

    // Save to file
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_append(ff, PROTOPIRATE_SAVE_FILE)) {
            // If file doesn't exist, create it
            if(!flipper_format_file_open_always(ff, PROTOPIRATE_SAVE_FILE)) break;
        }

        FuriString* temp = furi_string_alloc();

        // Write signal data
        flipper_format_write_string_cstr(ff, "Protocol",
            app->last_result.protocol ? app->last_result.protocol->name : "unknown");

        furi_string_printf(temp, "%lu", app->last_result.serial);
        flipper_format_write_string(ff, "Serial", temp);

        furi_string_printf(temp, "%lu", app->last_result.button);
        flipper_format_write_string(ff, "Button", temp);

        furi_string_printf(temp, "%lu", app->last_result.counter);
        flipper_format_write_string(ff, "Counter", temp);

        furi_string_printf(temp, "%lu", app->last_result.crc);
        flipper_format_write_string(ff, "CRC", temp);

        furi_string_printf(temp, "%lu", app->last_result.fixed_code);
        flipper_format_write_string(ff, "FixedCode", temp);

        furi_string_printf(temp, "%lu", app->last_result.frequency);
        flipper_format_write_string(ff, "Frequency", temp);

        furi_string_free(temp);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void protopirate_storage_save_all(ProtoPirateApp* app) {
    furi_assert(app);

    protopirate_storage_ensure_dir();
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Overwrite file with all saved signals
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(ff, PROTOPIRATE_SAVE_FILE)) break;

        FuriString* temp = furi_string_alloc();

        for(uint32_t i = 0; i < app->saved_count; i++) {
            flipper_format_write_string_cstr(ff, "Protocol",
                app->saved_signals[i].result.protocol ?
                app->saved_signals[i].result.protocol->name : "unknown");

            furi_string_printf(temp, "%lu", app->saved_signals[i].result.serial);
            flipper_format_write_string(ff, "Serial", temp);

            furi_string_printf(temp, "%lu", app->saved_signals[i].result.button);
            flipper_format_write_string(ff, "Button", temp);

            furi_string_printf(temp, "%lu", app->saved_signals[i].result.counter);
            flipper_format_write_string(ff, "Counter", temp);

            furi_string_printf(temp, "%lu", app->saved_signals[i].result.crc);
            flipper_format_write_string(ff, "CRC", temp);

            furi_string_printf(temp, "%lu", app->saved_signals[i].result.fixed_code);
            flipper_format_write_string(ff, "FixedCode", temp);
        }

        furi_string_free(temp);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void protopirate_storage_load_saved(ProtoPirateApp* app) {
    furi_assert(app);

    app->saved_count = 0;

    protopirate_storage_ensure_dir();
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Check if file exists
    if(!storage_file_exists(storage, PROTOPIRATE_SAVE_FILE)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_existing(ff, PROTOPIRATE_SAVE_FILE)) break;

        FuriString* temp = furi_string_alloc();

        while(app->saved_count < 100) {
            // Read protocol
            if(!flipper_format_read_string(ff, "Protocol", temp)) break;
            SavedSignal* signal = &app->saved_signals[app->saved_count];
            memset(signal, 0, sizeof(SavedSignal));

            // Look up protocol by name
            snprintf(signal->name, sizeof(signal->name), "%s", furi_string_get_cstr(temp));
            signal->result.protocol = protopirate_find_protocol_by_name(furi_string_get_cstr(temp));

            // Read numeric fields
            FuriString* val = furi_string_alloc();

            if(flipper_format_read_string(ff, "Serial", val))
                signal->result.serial = atol(furi_string_get_cstr(val));
            if(flipper_format_read_string(ff, "Button", val))
                signal->result.button = atol(furi_string_get_cstr(val));
            if(flipper_format_read_string(ff, "Counter", val))
                signal->result.counter = atol(furi_string_get_cstr(val));
            if(flipper_format_read_string(ff, "CRC", val))
                signal->result.crc = atol(furi_string_get_cstr(val));
            if(flipper_format_read_string(ff, "FixedCode", val))
                signal->result.fixed_code = atol(furi_string_get_cstr(val));
            if(flipper_format_read_string(ff, "Frequency", val))
                signal->result.frequency = atol(furi_string_get_cstr(val));

            signal->result.valid = true;
            signal->has_raw_data = false;

            furi_string_free(val);
            app->saved_count++;
        }

        furi_string_free(temp);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

bool protopirate_storage_delete_signal(ProtoPirateApp* app, uint32_t index) {
    furi_assert(app);
    if(index >= app->saved_count) return false;

    // Shift remaining entries
    memmove(&app->saved_signals[index], &app->saved_signals[index + 1],
            (app->saved_count - index - 1) * sizeof(SavedSignal));
    app->saved_count--;

    // Save updated list
    protopirate_storage_save_all(app);
    return true;
}
