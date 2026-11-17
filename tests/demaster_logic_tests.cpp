// ============================================================
// THE DEMASTER'S PURE LOGIC: WHICH CONTENT IS INSTALLED, AND THE SIZE OF EVERY MIP LEVEL IT SENDS
// Built and run by tools/run_tests.cmd (x86, no GL, no game). A failure names what went wrong and exits with 1.
// ============================================================
#include "../ff8_demaster/content_profile.h"
#include "../ff8_demaster/file_read.h"
#include "../ff8_demaster/image_header.h"
#include "../ff8_demaster/upload_levels.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

namespace {
int failures = 0;
void Require(bool value, const std::string& what) {
    if (value) return;
    std::printf("FAIL: %s\n", what.c_str());
    ++failures;
}

// A file that starts like a PNG of the given size (only the IHDR is read).
void WritePng(const std::filesystem::path& file, uint32_t width, uint32_t height) {
    const unsigned char header[24]{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R',
        static_cast<unsigned char>(width >> 24), static_cast<unsigned char>(width >> 16), static_cast<unsigned char>(width >> 8),
        static_cast<unsigned char>(width), static_cast<unsigned char>(height >> 24), static_cast<unsigned char>(height >> 16),
        static_cast<unsigned char>(height >> 8), static_cast<unsigned char>(height)};
    std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(header), sizeof header);
}
void WriteFile(const std::filesystem::path& file) { std::ofstream(file, std::ios::binary) << "DDS "; }

// ============================================================
// Every mip level goes with its own size: a 2048x1024 chain of 12 levels is 2048x1024, 1024x512, ... 2x1, 1x1.
// The upstream loop sent 2048x1024 for every level (GL refused the compressed ones past the first).
// ============================================================
void MipLevels() {
    struct Sent {uint32_t level, width, height, size;};
    std::vector<Sent> sent;
    const unsigned char bytes[16]{};
    demaster::SendLevels(2048, 1024, 12,
        [&](uint32_t level, const void*& data, uint32_t& size) {
            if (level == 5) return false;   // a level bimg cannot give is skipped, the others still go
            data = bytes;
            size = 100 + level;
            return true;
        },
        [&](uint32_t level, uint32_t width, uint32_t height, const void* data, uint32_t size) {
            Require(data == bytes, "a level was sent without the bytes read for it");
            sent.push_back({level, width, height, size});
        });
    Require(sent.size() == 11, "12 levels with one unreadable should send 11, sent " + std::to_string(sent.size()));
    for (const auto& level : sent) {
        const uint32_t width = (std::max)(1u, 2048u >> level.level), height = (std::max)(1u, 1024u >> level.level);
        Require(level.width == width && level.height == height,
                "level " + std::to_string(level.level) + " went as " + std::to_string(level.width) + "x" +
                std::to_string(level.height) + ", its own size is " + std::to_string(width) + "x" + std::to_string(height));
        Require(level.size == 100 + level.level, "level " + std::to_string(level.level) + " went with another level's byte count");
        Require(level.level != 5, "the unreadable level 5 was sent");
    }
    Require(demaster::LevelExtent(1536, 10) == 1 && demaster::LevelExtent(1536, 9) == 3 && demaster::LevelExtent(7, 40) == 1,
            "level extents of 1536 (levels 9 and 10) or past 32 levels are wrong");
}

// ============================================================
// The names Lunar Cry 4's sub-image replacement packs (texture.cpp, d2f6320): magNNN_K.dds and c0m121/122/126_K.dds.
// ============================================================
void PackedNames() {
    Require(demaster::PackedChunk("mag005_0.dds") == 0 && demaster::PackedChunk("MAG183_12.DDS") == 12,
            "a MAG chunk's .dds was not read");
    Require(demaster::PackedChunk("c0m121_4.dds") == 4 && demaster::PackedChunk("c0m126_2.dds") == 2,
            "an Ultimecia chunk's .dds was not read");
    Require(demaster::PackedChunk("c0m123_0.dds") == -1 && demaster::PackedChunk("c0m095_0.dds") == -1,
            "a monster Lunar Cry 4 does not pack was read as packed");
    Require(demaster::PackedChunk("mag005_0.png") == -1 && demaster::PackedChunk("mag089_2_jp.dds") == -1 &&
            demaster::PackedChunk("mag05_0.dds") == -1 && demaster::PackedChunk("mag005_.dds") == -1 &&
            demaster::PackedChunk("a0stg001_16.dds") == -1,
            "a name that is no packed chunk was read as one");
}

// ============================================================
// The content profile from a hd_new folder: an even chunk's .dds beside a 4x4 .png is Lunar Cry 4 or later.
// ============================================================
void Profiles(const std::filesystem::path& root) {
    using demaster::ContentProfile;
    const auto folder = [&root](const char* name) {
        const auto path = root / name;
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
        return path;
    };
    uint32_t width = 0, height = 0;
    {
        const auto png = folder("png") / "a.png";
        WritePng(png, 4, 4);
        Require(demaster::PngSize(png, width, height) && width == 4 && height == 4, "a 4x4 PNG header was not read");
        WriteFile(root / "png" / "b.png");
        Require(!demaster::PngSize(root / "png" / "b.png", width, height), "a file that is no PNG was read as one");
    }
    {
        const auto lc4 = folder("lc4");
        WriteFile(lc4 / "mag005_0.dds");
        WritePng(lc4 / "mag005_0.png", 4, 4);
        WritePng(lc4 / "mag005_1.png", 4, 4);
        Require(demaster::DetectContent(lc4) == ContentProfile::LunarCry4, "Lunar Cry 4 MAG packing was not recognized");
    }
    {
        const auto ultimecia = folder("ultimecia");
        WriteFile(ultimecia / "c0m121_2.dds");
        WritePng(ultimecia / "c0m121_2.png", 4, 4);
        Require(demaster::DetectContent(ultimecia) == ContentProfile::LunarCry4, "Lunar Cry 4 Ultimecia packing was not recognized");
    }
    {
        const auto standard = folder("standard");
        WritePng(standard / "mag005_0.png", 512, 256);
        WritePng(standard / "mag005_1.png", 512, 256);
        WriteFile(standard / "c0m095_0.dds");
        Require(demaster::DetectContent(standard) == ContentProfile::Standard, "standard content was taken for Lunar Cry 4");
    }
    {
        const auto realArt = folder("real-art");
        WriteFile(realArt / "mag005_0.dds");
        WritePng(realArt / "mag005_0.png", 512, 256);
        Require(demaster::DetectContent(realArt) == ContentProfile::Standard, "a .dds beside real art was taken for Lunar Cry 4");
    }
    {
        const auto odd = folder("odd");
        WriteFile(odd / "mag005_1.dds");
        WritePng(odd / "mag005_1.png", 4, 4);
        Require(demaster::DetectContent(odd) == ContentProfile::Standard, "an odd chunk's .dds was taken for Lunar Cry 4 packing");
    }
    Require(demaster::DetectContent(root / "missing") == ContentProfile::Standard, "a missing folder was not standard content");
    const auto lc4 = root / "lc4";
    Require(demaster::ChooseContent(0, lc4) == ContentProfile::Standard && demaster::ChooseContent(1, root / "standard") == ContentProfile::LunarCry4 &&
            demaster::ChooseContent(-1, lc4) == ContentProfile::LunarCry4 && demaster::ChooseContent(7, root / "standard") == ContentProfile::Standard,
            "CONTENT_PROFILE 0, 1 or the files' choice was not honoured");
}

// ============================================================
// The battle character atlas is sized from the first texture's header alone (image_header.h): a PNG's IHDR, a DDS
// header's height then width; a missing file or a cut one gives zero. The monster and GF atlases keep their side.
// ============================================================
void ImageHeaders(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    WritePng(root / "wide.png", 1536, 576);
    const auto png = ReadImageHeaderSize((root / "wide.png").string().c_str());
    Require(png.width == 1536 && png.height == 576, "a PNG's size was not read from its IHDR");
    {
        uint32_t header[5]{0x20534444, 124, 0x1007, 512, 1024};   // "DDS ", dwSize, dwFlags, dwHeight, dwWidth
        std::ofstream(root / "wide.dds", std::ios::binary).write(reinterpret_cast<const char*>(header), sizeof header);
    }
    const auto dds = ReadImageHeaderSize((root / "wide.dds").string().c_str());
    Require(dds.width == 1024 && dds.height == 512 && dds.dds && !png.dds, "a DDS's size was not read as height then width");
    // The battle atlases keep the side they were always sized from: a PNG's width, a DDS's height (the upstream reader
    // took dwHeight for the width); and lastGetImageResolution's view of a DDS keeps both sides as it saw them.
    Require(AtlasSide(png) == 1536 && AtlasSide(dds) == 512 && AtlasSize(dds).width == 512 && AtlasSize(dds).height == 1024,
            "the side a battle atlas is sized from changed for a PNG or a DDS");
    WriteFile(root / "short.dds");   // "DDS " alone
    const auto none = ReadImageHeaderSize((root / "missing.png").string().c_str());
    const auto shortOne = ReadImageHeaderSize((root / "short.dds").string().c_str());
    Require(!none.width && !none.height && !shortOne.width && !shortOne.height, "a missing or cut file gave a size");
}

// ============================================================
// A texture file is read whole in one read (file_read.h): every byte as on disk, larger than one read of the old
// stream's buffer; a missing or empty file gives nothing (LoadImageFromFile then returns no image, as before).
// ============================================================
void WholeFiles(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    std::vector<char> written(3 * 1024 * 1024 + 17);
    for (size_t i = 0; i < written.size(); ++i) written[i] = char(i * 31 + (i >> 11));
    std::ofstream(root / "big.dds", std::ios::binary).write(written.data(), std::streamsize(written.size()));
    std::ofstream(root / "empty.dds", std::ios::binary).close();
    std::unique_ptr<char[]> bytes;
    size_t size = 0;
    Require(ReadWholeFile((root / "big.dds").string().c_str(), bytes, size) && size == written.size() &&
                std::memcmp(bytes.get(), written.data(), size) == 0,
            "a file was not read whole with its bytes");
    Require(!ReadWholeFile((root / "missing.dds").string().c_str(), bytes, size) && !bytes && !size, "a missing file gave bytes");
    Require(!ReadWholeFile((root / "empty.dds").string().c_str(), bytes, size) && !bytes && !size, "an empty file gave bytes");
}
}

int main(int count, char** arguments) {
    if (count != 2) {
        std::printf("usage: demaster_logic_tests <scratch folder>\n");
        return 2;
    }
    const auto root = std::filesystem::absolute(arguments[1]) / std::to_string(GetCurrentProcessId());
    MipLevels();
    PackedNames();
    Profiles(root);
    ImageHeaders(root / "image-headers");
    WholeFiles(root / "whole-files");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (failures) return 1;
    std::printf("Demaster logic: mip level sizes, packed chunk names, content profiles, image header sizes and whole-file reads passed\n");
    return 0;
}
