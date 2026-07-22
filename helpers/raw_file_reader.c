#include "raw_file_reader.h"
#include "../protopirate_app_i.h"
#include "../defines.h"
#include "../protocols/protocol_items.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <lib/toolbox/path.h>
#include <lib/subghz/protocols/raw.h>

bool protopirate_raw_file_decode(FuriString* file_path, DecodeResult* result) {
    if(!file_path || !result) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;

    do {
        if(!flipper_format_file_open_existing(ff, file_path)) break;

        FuriString* temp = furi_string_alloc();
        uint32_t temp_u32;

        // Read frequency
        if(!flipper_format_read_uint32(ff, "Frequency", &temp_u32, 1)) break;
        result->frequency = temp_u32;

        // Read preset
        if(!flipper_format_read_string(ff, "Preset", temp)) break;

        // Determine protocol type from filename or content
        FuriString* filename = furi_string_alloc();
        path_extract_filename(file_path, filename, true);
        furi_string_trim(filename);

        // Try to match protocol by filename
        result->protocol = protopirate_find_protocol_by_name(furi_string_get_cstr(filename));

        // If no protocol match, try generic decode
        if(!result->protocol) {
            // Read raw data
            // Try custom protocol fields
            if(flipper_format_read_uint32(ff, "Serial", &temp_u32, 1)) {
                result->serial = temp_u32;
            }
            if(flipper_format_read_uint32(ff, "Button", &temp_u32, 1)) {
                result->button = temp_u32;
            }
            if(flipper_format_read_uint32(ff, "Counter", &temp_u32, 1)) {
                result->counter = temp_u32;
            }
            if(flipper_format_read_uint32(ff, "CRC", &temp_u32, 1)) {
                result->crc = temp_u32;
            }

            // Attempt to load raw pulses and decode
            uint32_t pulses[PROTOPIRATE_MAX_PULSES];
            uint32_t num_pulses = 0;
            if(protopirate_raw_file_load_pulses(file_path, pulses, &num_pulses)) {
                // Try to decode from pulses
                DecodeResult decode_result;
                if(protopirate_try_decode_all(pulses, num_pulses, &decode_result)) {
                    memcpy(result, &decode_result, sizeof(DecodeResult));
                }
            }
        }

        result->valid = true;
        success = true;

        furi_string_free(filename);
        furi_string_free(temp);
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    return success;
}

bool protopirate_raw_file_load_pulses(FuriString* file_path, uint32_t* pulses, uint32_t* num_pulses) {
    if(!file_path || !pulses || !num_pulses) return false;

    *num_pulses = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool success = false;

    do {
        if(!flipper_format_file_open_existing(ff, file_path)) break;

        uint32_t data_size = 0;
        if(!flipper_format_get_value_count(ff, "RAW_Data", &data_size)) break;

        if(data_size > PROTOPIRATE_MAX_PULSES) {
            data_size = PROTOPIRATE_MAX_PULSES;
        }

        if(!flipper_format_read_uint32(ff, "RAW_Data", pulses, data_size)) break;

        *num_pulses = data_size;
        success = true;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    return success;
}
