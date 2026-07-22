#include "protocol_items.h"
#include "protocols_common.h"
#include "../defines.h"

#include <furi.h>
#include <string.h>

// Global protocol registry
ProtocolEntry* g_protocols[PROTOCOL_MAX_COUNT];
uint32_t g_protocol_count = 0;

// Register a protocol
static void protopirate_register_protocol(
    Manufacturer manufacturer,
    uint8_t variant,
    const char* name,
    const char* display_name,
    uint16_t features,
    uint8_t packet_len_bits,
    ProtocolDecodeFunc decode,
    ProtocolBuildFunc build) {

    if(g_protocol_count >= PROTOCOL_MAX_COUNT) return;

    ProtocolEntry* entry = malloc(sizeof(ProtocolEntry));
    memset(entry, 0, sizeof(ProtocolEntry));

    entry->version.manufacturer = manufacturer;
    entry->version.variant = variant;
    entry->version.name = name;
    entry->version.display_name = display_name;
    entry->version.features = features;
    entry->version.packet_len_bits = packet_len_bits;
    entry->decode = decode;
    entry->build = build;

    g_protocols[g_protocol_count] = entry;
    g_protocol_count++;
}

void protopirate_protocols_init(void) {
    // Reset
    g_protocol_count = 0;

    // Kia protocols (KeeLoq with rollback)
    protopirate_register_protocol(
        ManufacturerKia, 0, "KiaV0", "Kia V0 (KeeLoq)",
        PROTO_HAS_TX | PROTO_HAS_ROLLBACK | PROTO_HAS_CRC | PROTO_HAS_COUNTER |
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v0_decode, kia_v0_build);

    protopirate_register_protocol(
        ManufacturerKia, 1, "KiaV1", "Kia V1 (KeeLoq)",
        PROTO_HAS_TX | PROTO_HAS_ROLLBACK | PROTO_HAS_CRC | PROTO_HAS_COUNTER |
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v1_decode, kia_v1_build);

    protopirate_register_protocol(
        ManufacturerKia, 2, "KiaV2", "Kia V2",
        PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v2_decode, NULL);

    protopirate_register_protocol(
        ManufacturerKia, 3, "KiaV3V4", "Kia V3/V4",
        PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v3_v4_decode, NULL);

    protopirate_register_protocol(
        ManufacturerKia, 5, "KiaV5", "Kia V5",
        PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v5_decode, NULL);

    protopirate_register_protocol(
        ManufacturerKia, 6, "KiaV6", "Kia V6",
        PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v6_decode, NULL);

    protopirate_register_protocol(
        ManufacturerKia, 7, "KiaV7", "Kia V7",
        PROTO_HAS_CRC | PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_KEE_LOQ,
        66, kia_v7_decode, NULL);

    // Ford protocols
    protopirate_register_protocol(
        ManufacturerFord, 0, "FordV0", "Ford V0",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON,
        48, ford_v0_decode, NULL);

    protopirate_register_protocol(
        ManufacturerFord, 1, "FordV1", "Ford V1",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON,
        48, ford_v1_decode, NULL);

    protopirate_register_protocol(
        ManufacturerFord, 2, "FordV2", "Ford V2",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON,
        48, ford_v2_decode, NULL);

    protopirate_register_protocol(
        ManufacturerFord, 3, "FordV3", "Ford V3",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON,
        48, ford_v3_decode, NULL);

    // Subaru
    protopirate_register_protocol(
        ManufacturerSubaru, 0, "Subaru", "Subaru",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED,
        40, subaru_decode, NULL);

    // Fiat
    protopirate_register_protocol(
        ManufacturerFiat, 0, "FiatV0", "Fiat V0",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        60, fiat_v0_decode, NULL);

    protopirate_register_protocol(
        ManufacturerFiat, 1, "FiatV1", "Fiat V1",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        60, fiat_v1_decode, NULL);

    protopirate_register_protocol(
        ManufacturerFiat, 2, "FiatV2", "Fiat V2",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        60, fiat_v2_decode, NULL);

    // Chrysler
    protopirate_register_protocol(
        ManufacturerChrysler, 0, "ChryslerV0", "Chrysler V0",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED,
        40, chrysler_v0_decode, NULL);

    // StarLine
    protopirate_register_protocol(
        ManufacturerStarLine, 0, "StarLine", "StarLine",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER | PROTO_HAS_CRC,
        80, star_line_decode, NULL);

    // Scher-Khan
    protopirate_register_protocol(
        ManufacturerScherKhan, 0, "ScherKhan", "Scher-Khan",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER | PROTO_HAS_CRC,
        64, scher_khan_decode, NULL);

    // Mitsubishi
    protopirate_register_protocol(
        ManufacturerMitsubishi, 0, "MitsubishiV0", "Mitsubishi V0",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED,
        32, mitsubishi_v0_decode, NULL);

    // Mazda
    protopirate_register_protocol(
        ManufacturerMazda, 0, "MazdaV0", "Mazda V0",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_COUNTER,
        48, mazda_v0_decode, NULL);

    // Honda
    protopirate_register_protocol(
        ManufacturerHonda, 1, "HondaV1", "Honda V1",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        64, honda_v1_decode, NULL);

    protopirate_register_protocol(
        ManufacturerHonda, 2, "HondaV2", "Honda V2",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        64, honda_v2_decode, NULL);

    protopirate_register_protocol(
        ManufacturerHonda, 0, "HondaStatic", "Honda Static",
        PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_FIXED,
        32, honda_static_decode, NULL);

    // PSA
    protopirate_register_protocol(
        ManufacturerPSA, 0, "PSA", "PSA (Peugeot/Citroen)",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        56, psa_decode, NULL);

    // Porsche Touareg
    protopirate_register_protocol(
        ManufacturerPorsche, 0, "PorscheTouareg", "Porsche Touareg",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        60, porsche_touareg_decode, NULL);

    // Renault
    protopirate_register_protocol(
        ManufacturerRenault, 0, "RenaultV0", "Renault V0",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        56, renault_v0_decode, NULL);

    // VAG
    protopirate_register_protocol(
        ManufacturerVAG, 0, "VAG", "VAG (VW/Audi/Seat/Skoda)",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        64, vag_decode, NULL);

    // AUT64
    protopirate_register_protocol(
        ManufacturerAUT64, 0, "AUT64", "AUT64",
        PROTO_HAS_COUNTER | PROTO_HAS_SERIAL | PROTO_HAS_BUTTON | PROTO_HAS_CRC,
        56, aut64_decode, NULL);
}

ProtocolVersion* protopirate_find_protocol_by_name(const char* name) {
    if(!name) return NULL;

    for(uint32_t i = 0; i < g_protocol_count; i++) {
        if(g_protocols[i] && strcmp(g_protocols[i]->version.name, name) == 0) {
            return &g_protocols[i]->version;
        }
    }
    return NULL;
}

bool protopirate_try_decode_all(const uint32_t* pulses, uint32_t num_pulses, DecodeResult* result) {
    if(!pulses || !result || num_pulses < 10) return false;

    for(uint32_t i = 0; i < g_protocol_count; i++) {
        if(g_protocols[i] && g_protocols[i]->decode) {
            DecodeResult test_result;
            memset(&test_result, 0, sizeof(DecodeResult));
            test_result.protocol = &g_protocols[i]->version;

            if(g_protocols[i]->decode(pulses, num_pulses, &test_result)) {
                memcpy(result, &test_result, sizeof(DecodeResult));
                return true;
            }
        }
    }

    return false;
}

ProtocolEntry* protopirate_get_protocol(uint32_t index) {
    if(index < g_protocol_count) return g_protocols[index];
    return NULL;
}

uint32_t protopirate_get_protocol_count(void) {
    return g_protocol_count;
}
