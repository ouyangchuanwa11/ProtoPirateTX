#include "protopirate_settings.h"
#include "../defines.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define SETTINGS_FILE "/ext/apps_data/protopirate/settings.txt"

void protopirate_settings_defaults(ProtoPirateSettings* settings) {
    *settings = (ProtoPirateSettings)PROTOPIRATE_SETTINGS_DEFAULT;
}

void protopirate_settings_load(ProtoPirateSettings* settings) {
    furi_assert(settings);

    // Set defaults first
    protopirate_settings_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, "/ext/apps_data/protopirate");

    if(!storage_file_exists(storage, SETTINGS_FILE)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_FILE)) break;

        FuriString* val = furi_string_alloc();

        if(flipper_format_read_string(ff, "Frequency", val))
            settings->last_frequency = atol(furi_string_get_cstr(val));
        if(flipper_format_read_string(ff, "Band", val))
            settings->last_band = atol(furi_string_get_cstr(val));
        if(flipper_format_read_string(ff, "TimingFactor", val))
            settings->timing_factor = atof(furi_string_get_cstr(val));
        if(flipper_format_read_string(ff, "TXRepeat", val))
            settings->tx_repeat_count = atol(furi_string_get_cstr(val));

        furi_string_free(val);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void protopirate_settings_save(ProtoPirateSettings* settings) {
    furi_assert(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data/protopirate");

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_FILE)) break;

        FuriString* temp = furi_string_alloc();

        furi_string_printf(temp, "%lu", settings->last_frequency);
        flipper_format_write_string(ff, "Frequency", temp);

        furi_string_printf(temp, "%lu", settings->last_band);
        flipper_format_write_string(ff, "Band", temp);

        furi_string_printf(temp, "%.1f", (double)settings->timing_factor);
        flipper_format_write_string(ff, "TimingFactor", temp);

        furi_string_printf(temp, "%lu", settings->tx_repeat_count);
        flipper_format_write_string(ff, "TXRepeat", temp);

        furi_string_free(temp);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
