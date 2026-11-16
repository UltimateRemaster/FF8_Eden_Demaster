#pragma once
// ============================================================
// WHICH LUNAR CRY CONTENT IS INSTALLED, READ FROM THE FILES THE DEMASTER LOADS
//
// The Demaster of Lunar Cry 4 (upstream commit d2f6320) expects MAG art and
// Ultimecia's c0m121/122/126 art packed as one .dds for each even chunk, with a
// 4x4 .png left as a placeholder for every chunk: it uploads that .dds when the
// engine sends the chunk by glTexSubImage2D, drops the odd chunk, halves the MAG
// atlas width and keeps the engine's own monster atlas size. The standard
// Demaster 1.3.7 expects the real art in the .png and does none of that
// (docs/LUNARCRY4_DEMASTER_DIFF.md, 4.2 and 5.7). So the content tells which of
// the two behaviours it was made for: an even chunk's .dds beside a 4x4 .png of
// the same name is Lunar Cry 4 or later.
//
// Pure logic, no GL and no game state: tests/demaster_logic_tests.cpp.
// ============================================================
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace demaster {
enum class ContentProfile : int { Standard = 0, LunarCry4 = 1 };

// The width and height in a PNG's IHDR; false when the file cannot be read or is no PNG.
inline bool PngSize(const std::filesystem::path& file, uint32_t& width, uint32_t& height) {
    std::ifstream in(file, std::ios::binary);
    unsigned char header[24]{};
    if (!in.read(reinterpret_cast<char*>(header), sizeof header)) return false;
    static constexpr unsigned char signature[8]{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int at = 0; at < 8; ++at)
        if (header[at] != signature[at]) return false;
    if (header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R') return false;
    width = uint32_t(header[16]) << 24 | uint32_t(header[17]) << 16 | uint32_t(header[18]) << 8 | header[19];
    height = uint32_t(header[20]) << 24 | uint32_t(header[21]) << 16 | uint32_t(header[22]) << 8 | header[23];
    return true;
}

// The chunk of a packed file name: "magNNN_K.dds" or "c0m121_K.dds", "c0m122_K.dds", "c0m126_K.dds", in any case.
// -1 for any other name.
inline int PackedChunk(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (const char c : name) lower += char(std::tolower(static_cast<unsigned char>(c)));
    const auto digit = [&lower](size_t at) { return at < lower.size() && std::isdigit(static_cast<unsigned char>(lower[at])); };
    const bool mag = lower.compare(0, 3, "mag") == 0 && digit(3) && digit(4) && digit(5);
    const bool ultimecia = lower.compare(0, 6, "c0m121") == 0 || lower.compare(0, 6, "c0m122") == 0 ||
                           lower.compare(0, 6, "c0m126") == 0;
    if ((!mag && !ultimecia) || lower.size() < 8 || lower[6] != '_') return -1;
    size_t at = 7;
    int chunk = 0;
    while (digit(at) && at < 11) chunk = chunk * 10 + (lower[at++] - '0');
    if (at == 7 || lower.compare(at, std::string::npos, ".dds") != 0) return -1;
    return chunk;
}

// Lunar Cry 4 or later: some even chunk's .dds stands beside a 4x4 .png of the same name in `hdNew`
// (DEMASTER_EXP\textures\battle.fs\hd_new). Anything else, a missing folder included, is standard content.
inline ContentProfile DetectContent(const std::filesystem::path& hdNew) {
    std::error_code error;
    std::filesystem::directory_iterator entry(hdNew, error);
    for (const std::filesystem::directory_iterator end; !error && entry != end; entry.increment(error)) {
        const auto& path = entry->path();
        const int chunk = PackedChunk(path.filename().string());
        if (chunk < 0 || chunk % 2) continue;
        auto placeholder = path;
        placeholder.replace_extension(".png");
        uint32_t width = 0, height = 0;
        if (PngSize(placeholder, width, height) && width == 4 && height == 4) return ContentProfile::LunarCry4;
    }
    return ContentProfile::Standard;
}

// The profile in use: `configured` 0 or 1 is the user's choice in demaster.conf (CONTENT_PROFILE); any other value
// (the default, -1) lets the files decide.
inline ContentProfile ChooseContent(int configured, const std::filesystem::path& hdNew) {
    if (configured == 0) return ContentProfile::Standard;
    if (configured == 1) return ContentProfile::LunarCry4;
    return DetectContent(hdNew);
}
}
