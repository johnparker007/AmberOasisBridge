#ifndef AMBER_API_V2_PROPOSAL_H
#define AMBER_API_V2_PROPOSAL_H

/* Design-only contract. AmberGetApi does not yet negotiate this version. */
#include "amber_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMBER_API_VERSION_2 0x00020000u
#define AMBER_OUTPUT_SNAPSHOT_VERSION_1 1u
#define AMBER_CAPABILITIES_VERSION_1 1u
#define AMBER_AUDIO_FORMAT_VERSION_1 1u
#define AMBER_REEL_CONFIGURATION_VERSION_1 1u
#define AMBER_COIN_CONFIGURATION_VERSION_1 1u

#define AMBER_MAX_SWITCHES 256u
#define AMBER_MAX_MATRIX_LAMPS 512u
#define AMBER_MAX_REELS 8u
#define AMBER_MAX_ALPHA_DISPLAYS 2u
#define AMBER_ALPHA_CHARACTERS 16u
#define AMBER_MAX_SEVEN_SEGMENT_DISPLAYS 40u
#define AMBER_MAX_COIN_CHANNELS 6u
#define AMBER_MAX_COIN_ROUTES 8u

#define AMBER_CAP_SWITCH_INPUT       UINT64_C(0x0000000000000001)
#define AMBER_CAP_OUTPUT_SNAPSHOT    UINT64_C(0x0000000000000002)
#define AMBER_CAP_AUDIO              UINT64_C(0x0000000000000004)
#define AMBER_CAP_REEL_CONFIGURATION UINT64_C(0x0000000000000008)
#define AMBER_CAP_COIN_CONFIGURATION UINT64_C(0x0000000000000010)
#define AMBER_CAP_PERCENT_SWITCH     UINT64_C(0x0000000000000020)

#define AMBER_AUDIO_SAMPLE_PCM_S16 1u
#define AMBER_AUDIO_INTERLEAVED 1u
#define AMBER_ALPHA_DECIMAL_POINT UINT8_C(0x01)
#define AMBER_ALPHA_COMMA_TAIL UINT8_C(0x02)
#define AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT UINT32_C(0x00000001)

/* Proposed append-only AmberResult values; v1's declaration is unchanged. */
#define AMBER_NOT_SUPPORTED ((AmberResult)11)
#define AMBER_INVALID_RANGE ((AmberResult)12)
#define AMBER_MALFORMED_CONFIGURATION ((AmberResult)13)

typedef struct AmberCapabilitiesV1 {
    uint32_t struct_size;
    uint32_t version;
    uint64_t feature_bits;
    uint32_t max_switches;
    uint32_t reserved[3];
} AmberCapabilitiesV1;

typedef struct AmberLampStateV1 {
    uint32_t is_on;
    uint32_t brightness_q16_16;
} AmberLampStateV1;

typedef struct AmberAlphaDisplayStateV1 {
    uint16_t segment_masks[AMBER_ALPHA_CHARACTERS];
    uint8_t dot_comma[AMBER_ALPHA_CHARACTERS];
    uint32_t brightness_q16_16;
} AmberAlphaDisplayStateV1;

typedef struct AmberSevenSegmentStateV1 {
    uint32_t segment_mask;
    uint32_t brightness_q16_16;
} AmberSevenSegmentStateV1;

typedef struct AmberOutputSnapshotV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t matrix_lamp_count;
    uint32_t reel_count;
    uint32_t alpha_display_count;
    uint32_t seven_segment_display_count;
    uint32_t reserved[4];
    AmberLampStateV1 matrix_lamps[AMBER_MAX_MATRIX_LAMPS];
    int32_t reel_positions[AMBER_MAX_REELS];
    AmberAlphaDisplayStateV1 alpha_displays[AMBER_MAX_ALPHA_DISPLAYS];
    AmberSevenSegmentStateV1 seven_segment_displays[AMBER_MAX_SEVEN_SEGMENT_DISPLAYS];
} AmberOutputSnapshotV1;

typedef struct AmberAudioFormatV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t sample_format;
    uint32_t interleaving;
    uint32_t reserved[2];
} AmberAudioFormatV1;

typedef struct AmberReelConfigV1 {
    uint32_t reel_index;
    uint32_t enabled;
    uint32_t steps;
    uint32_t opto_start;
    uint32_t opto_end;
    uint32_t opto_invert;
} AmberReelConfigV1;

typedef struct AmberReelConfigurationV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t reel_count;
    uint32_t apply_mask;
    AmberReelConfigV1 reels[AMBER_MAX_REELS];
} AmberReelConfigurationV1;

typedef struct AmberCoinChannelConfigV1 {
    uint32_t channel_index;
    uint32_t enabled;
    uint32_t value;
    uint32_t lockout_invert;
    uint32_t reserved;
} AmberCoinChannelConfigV1;

typedef struct AmberCoinRouteConfigV1 {
    uint32_t route_index;
    uint32_t enabled;
    uint32_t counter_in;
    uint32_t counter_out;
    uint32_t port_index;
    uint32_t coin_code;
    uint32_t level;
    uint32_t full_level;
} AmberCoinRouteConfigV1;

typedef struct AmberCoinConfigurationV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t channel_apply_mask;
    uint32_t route_apply_mask;
    AmberCoinChannelConfigV1 channels[AMBER_MAX_COIN_CHANNELS];
    AmberCoinRouteConfigV1 routes[AMBER_MAX_COIN_ROUTES];
    uint32_t lockout_port_base;
    uint32_t lockout_port_value;
    uint32_t configuration_flags;
    uint32_t reserved;
} AmberCoinConfigurationV1;

typedef struct AmberApiV2 {
    /* Exact AmberApiV1 prefix, repeated rather than nested. */
    uint32_t struct_size;
    uint32_t api_version;
    AmberResult (AMBER_CALL *GetBridgeInfo)(AmberBridgeInfo* info);
    AmberResult (AMBER_CALL *EnumerateCore)(uint32_t index, AmberCoreInfo* info);
    AmberResult (AMBER_CALL *Create)(const char* core_id, AmberHandle* handle);
    AmberResult (AMBER_CALL *Destroy)(AmberHandle handle);
    AmberResult (AMBER_CALL *Initialise)(AmberHandle handle, const AmberInitialiseParams* params);
    AmberResult (AMBER_CALL *Reset)(AmberHandle handle);
    AmberResult (AMBER_CALL *Run)(AmberHandle handle, uint32_t cycles, int32_t* cycles_run);
    AmberResult (AMBER_CALL *Shutdown)(AmberHandle handle);
    AmberResult (AMBER_CALL *GetLastError)(AmberHandle handle, char* buffer, uint32_t capacity, uint32_t* required);
    AmberResult (AMBER_CALL *GetCapabilities)(AmberHandle handle, AmberCapabilitiesV1* capabilities);
    AmberResult (AMBER_CALL *SetSwitchState)(AmberHandle handle, uint32_t switch_index, uint32_t is_on);
    AmberResult (AMBER_CALL *GetOutputSnapshot)(AmberHandle handle, AmberOutputSnapshotV1* snapshot);
    AmberResult (AMBER_CALL *GetAudioFormat)(AmberHandle handle, AmberAudioFormatV1* format);
    AmberResult (AMBER_CALL *FillAudioFrames)(AmberHandle handle, int16_t* interleaved_samples, uint32_t frame_capacity, uint32_t* frames_written);
    AmberResult (AMBER_CALL *ConfigureReels)(AmberHandle handle, const AmberReelConfigurationV1* configuration);
    AmberResult (AMBER_CALL *ConfigureCoins)(AmberHandle handle, const AmberCoinConfigurationV1* configuration);
    AmberResult (AMBER_CALL *SetPercentageSwitch)(AmberHandle handle, uint32_t raw_switch_value);
} AmberApiV2;

#ifdef __cplusplus
}
#endif
#endif
