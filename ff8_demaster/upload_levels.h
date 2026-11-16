#pragma once
// ============================================================
// SEND EVERY MIP LEVEL OF AN IMAGE WITH THE LEVEL'S OWN SIZE
//
// glTexImage2D and glCompressedTexImage2D define the level they are given with the
// width and height they are given; level k of a W x H image is max(1, W >> k) by
// max(1, H >> k). The upstream loops (image.cpp in 1.3.7, in Lunar Cry 4, d2f6320,
// and in this fork until now) sent W x H for every level: a compressed level past
// the first was refused (its byte count does not match the size), an uncompressed
// one became a full-size level of its own bytes.
//
// Pure logic: the caller reads each level's bytes and makes the GL call.
// tests/demaster_logic_tests.cpp.
// ============================================================
#include <algorithm>
#include <cstdint>

namespace demaster {
inline uint32_t LevelExtent(uint32_t base, uint32_t level) noexcept {
    return level >= 32 ? 1u : (std::max)(1u, base >> level);
}

// For each level below `levels`: `read(level, data, size)` gives the level's bytes (false: the level is skipped, as
// the upstream loop skipped a level bimg could not give), then `send(level, width, height, data, size)` defines it.
template <class Read, class Send>
void SendLevels(uint32_t width, uint32_t height, uint32_t levels, Read&& read, Send&& send) {
    for (uint32_t level = 0; level < levels; ++level) {
        const void* data = nullptr;
        uint32_t size = 0;
        if (!read(level, data, size)) continue;
        send(level, LevelExtent(width, level), LevelExtent(height, level), data, size);
    }
}
}
