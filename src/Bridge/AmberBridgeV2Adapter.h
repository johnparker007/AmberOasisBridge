#ifndef AMBER_BRIDGE_V2_ADAPTER_H
#define AMBER_BRIDGE_V2_ADAPTER_H

#include "amber/amber_api.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace amber_v2 {
inline bool ToQ16_16(double value, uint32_t& output) noexcept {
    if (!std::isfinite(value)) return false;
    if (value <= 0.0) { output = 0; return true; }
    const double scaled = value * 65536.0;
    output = scaled >= static_cast<double>(UINT32_MAX)
        ? UINT32_MAX : static_cast<uint32_t>(std::floor(scaled + 0.5));
    return true;
}
inline uint8_t MapAlphaPunctuation(uint8_t native) noexcept {
    return native == static_cast<uint8_t>('.') ? AMBER_ALPHA_DECIMAL_POINT
         : native == static_cast<uint8_t>(',') ? AMBER_ALPHA_COMMA_TAIL : 0;
}
inline bool ConvertAlpha(const uint16_t segments[16], const uint8_t punctuation[16], double brightness,
                         AmberAlphaDisplayStateV1& output) noexcept {
    for(uint32_t character=0; character<16; ++character) {
        output.segment_masks[character]=segments[character];
        output.dot_comma[character]=MapAlphaPunctuation(punctuation[character]);
    }
    return ToQ16_16(brightness,output.brightness_q16_16);
}
inline uint32_t BuildSevenSegmentMask(const uint32_t on[8]) noexcept {
    uint32_t mask = 0;
    for (uint32_t segment=0; segment<8; ++segment) mask = (mask << 1) | (on[segment] ? 1u : 0u);
    return mask;
}
inline bool ConvertSevenSegmentPlane(const uint32_t on[256], const double brightness[256],
                                     AmberSevenSegmentStateV1 output[16]) noexcept {
    for(uint32_t display=0; display<16; ++display) {
        uint32_t cells[8]{}; double maximum=0.0;
        for(uint32_t segment=0; segment<8; ++segment) {
            const uint32_t source=display*16+segment;
            cells[segment]=on[source];
            if(brightness[source]>maximum) maximum=brightness[source];
        }
        output[display].segment_mask=BuildSevenSegmentMask(cells);
        if(!ToQ16_16(maximum,output[display].brightness_q16_16)) return false;
    }
    return true;
}
inline bool AudioExtent(uint32_t frames, uint32_t channels, uint32_t& samples, uint32_t& bytes) noexcept {
    const uint64_t sample_count=static_cast<uint64_t>(frames)*channels;
    const uint64_t byte_count=sample_count*sizeof(int16_t);
    if(sample_count>UINT32_MAX || byte_count>UINT32_MAX || byte_count>SIZE_MAX) return false;
    samples=static_cast<uint32_t>(sample_count); bytes=static_cast<uint32_t>(byte_count); return true;
}
inline void MergeReels(AmberReelConfigurationV1& retained, bool& present, const AmberReelConfigurationV1& update) noexcept {
    if(!present) { std::memset(&retained,0,sizeof(retained)); retained.struct_size=sizeof(retained); retained.version=AMBER_REEL_CONFIGURATION_VERSION_1; retained.reel_count=AMBER_MAX_REELS; }
    for(uint32_t i=0;i<AMBER_MAX_REELS;i++) if(update.apply_mask&(1u<<i)) retained.reels[i]=update.reels[i];
    retained.apply_mask|=update.apply_mask; present=true;
}
inline void MergeCoins(AmberCoinConfigurationV1& retained, bool& present, const AmberCoinConfigurationV1& update) noexcept {
    if(!present) { std::memset(&retained,0,sizeof(retained)); retained.struct_size=sizeof(retained); retained.version=AMBER_COIN_CONFIGURATION_VERSION_1; }
    for(uint32_t i=0;i<AMBER_MAX_COIN_CHANNELS;i++) if(update.channel_apply_mask&(1u<<i)) retained.channels[i]=update.channels[i];
    for(uint32_t i=0;i<AMBER_MAX_COIN_ROUTES;i++) if(update.route_apply_mask&(1u<<i)) retained.routes[i]=update.routes[i];
    retained.channel_apply_mask|=update.channel_apply_mask; retained.route_apply_mask|=update.route_apply_mask;
    if(update.configuration_flags&AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT) { retained.configuration_flags|=AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT; retained.lockout_port_base=update.lockout_port_base; retained.lockout_port_value=update.lockout_port_value; }
    present=true;
}
}
#endif
