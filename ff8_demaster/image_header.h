#pragma once
#include <cstdint>
#include <cstdio>

// ============================================================
// AN IMAGE FILE'S SIZE FROM ITS HEADER ALONE
// The battle character hook sized the atlas by decoding the whole first texture (LoadImageFromFile) and dropping it:
// a PNG of 768x768 decoded on every battle start for each party member. Its size is in the header:
//  - PNG: the signature (8 bytes), the IHDR chunk's length and type (8 bytes), then width and height, big-endian;
//  - DDS: "DDS " (4 bytes), then the header: dwSize (124), dwFlags, dwHeight, dwWidth, little-endian.
// Nothing global is written (GetImageResolutionFast keeps its result in lastGetImageResolution, which the Shiva atlas
// size reads). Zero width and height: the file is missing or neither format.
// ============================================================
struct ImageHeaderSize {
    uint32_t width = 0, height = 0;
    bool dds = false;
};

// ============================================================
// THE SIDE THE BATTLE ATLASES ARE SIZED FROM
// The upstream header reader took a DDS's dwHeight as its width (it skipped dwFlags, then read dwHeight into width and
// dwWidth into height); the monster and GF atlas hooks size the atlas from that "width". The sizes stay as they were:
// a PNG's width, a DDS's height. For a square image (every monster and GF first texture of the known content but two
// GF DDS, mag095_0 and mag184_0) both are the same.
// ============================================================
// The size as the atlas hooks have always seen it: width the side above, height the other one (the Shiva atlas reads
// both from lastGetImageResolution).
inline ImageHeaderSize AtlasSize(const ImageHeaderSize& size) {
    ImageHeaderSize seen = size;
    if (size.dds) {
        seen.width = size.height;
        seen.height = size.width;
    }
    return seen;
}
inline uint32_t AtlasSide(const ImageHeaderSize& size) { return AtlasSize(size).width; }

inline ImageHeaderSize ReadImageHeaderSize(const char* path) {
    ImageHeaderSize size;
    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || !file) return size;
    unsigned char bytes[24]{};
    const size_t read = fread(bytes, 1, sizeof(bytes), file);
    fclose(file);
    const auto big = [&](size_t at) {
        return (uint32_t(bytes[at]) << 24) | (uint32_t(bytes[at + 1]) << 16) | (uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
    };
    const auto little = [&](size_t at) {
        return uint32_t(bytes[at]) | (uint32_t(bytes[at + 1]) << 8) | (uint32_t(bytes[at + 2]) << 16) | (uint32_t(bytes[at + 3]) << 24);
    };
    static const unsigned char png[8]{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    bool isPng = read >= 24;
    for (int i = 0; i < 8 && isPng; ++i) isPng = bytes[i] == png[i];
    if (isPng && bytes[12] == 'I' && bytes[13] == 'H' && bytes[14] == 'D' && bytes[15] == 'R') {
        size.width = big(16);
        size.height = big(20);
        return size;
    }
    if (read >= 20 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ' && little(4) == 124) {
        size.height = little(12);
        size.width = little(16);
        size.dds = true;
    }
    return size;
}
