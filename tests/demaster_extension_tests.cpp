// ============================================================
// THE EXTENSION API OF A BUILT FFVIII_EDEN_DEMASTER.dll, READ WITHOUT RUNNING THE DEMASTER
// The DLL is mapped with DONT_RESOLVE_DLL_REFERENCES (no DllMain, no imports): DemasterGetExtensionV1 must still
// fill every field, each address inside the image, the import slot naming KERNEL32!GetFileAttributesExW, the
// trampoline slot in writable data, the monster table of 32-byte rows with names, and contentProfile -1.
// Built and run by tools/run_tests.cmd when .build\eden\out\FFVIII_EDEN_DEMASTER.dll exists.
// ============================================================
#include "../ff8_demaster/demaster_extension.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
int failures = 0;
void Require(bool value, const std::string& what) {
    if (value) return;
    std::printf("FAIL: %s\n", what.c_str());
    ++failures;
}
}

int main(int count, char** arguments) {
    if (count != 2) {
        std::printf("usage: demaster_extension_tests <FFVIII_EDEN_DEMASTER.dll>\n");
        return 2;
    }
    const HMODULE module = LoadLibraryExA(arguments[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!module) {
        std::printf("FAIL: the DLL could not be mapped (%lu)\n", GetLastError());
        return 1;
    }
    auto* const base = reinterpret_cast<const uint8_t*>(module);
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const uint32_t imageSize = nt->OptionalHeader.SizeOfImage;
    const auto inside = [&](const void* address) {
        const auto at = reinterpret_cast<uintptr_t>(address), begin = reinterpret_cast<uintptr_t>(base);
        return at >= begin && at < begin + imageSize;
    };
    const auto writable = [&](const void* address) {
        const auto rva = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(base));
        const auto* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section)
            if (rva >= section->VirtualAddress && rva < section->VirtualAddress + section->Misc.VirtualSize)
                return (section->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        return false;
    };
    using Get = const DemasterExtensionV1*(__cdecl*)();
    const auto get = reinterpret_cast<Get>(GetProcAddress(module, "DemasterGetExtensionV1"));
    Require(get != nullptr, "DemasterGetExtensionV1 is not exported");
    if (get) {
        const DemasterExtensionV1* extension = get();
        Require(extension && inside(extension), "the extension structure is not the DLL's own");
        if (extension) {
            Require(extension->size == sizeof(DemasterExtensionV1) && extension->version == 1,
                    "size " + std::to_string(extension->size) + " or version " + std::to_string(extension->version) + " is not this layout");
            Require(extension->contentProfile == -1, "contentProfile is decided without DllMain");
            const void* addresses[]{extension->loadImageFromFile, extension->createImageRgba8, extension->textureHash,
                extension->fileAttributesSlot, extension->texImage2DTrampolineSlot, extension->hookGlTexImage2D,
                extension->monsterTextureTable, extension->getAtlasResolutionGfs,
                reinterpret_cast<const void*>(extension->setWorldTextureObserver),
                reinterpret_cast<const void*>(extension->setReplacementUpload),
                reinterpret_cast<const void*>(extension->setUploadObserver),
                reinterpret_cast<const void*>(extension->subImageDropped), extension->imageGetRawData, extension->textureFormatTable};
            for (size_t index = 0; index < sizeof addresses / sizeof addresses[0]; ++index)
                Require(addresses[index] && inside(addresses[index]), "field " + std::to_string(index) + " is not an address of the DLL");
            // Imports are not resolved in this mapping: the slot still holds its hint/name entry.
            if (extension->fileAttributesSlot && inside(extension->fileAttributesSlot)) {
                const auto entry = *reinterpret_cast<const uint32_t*>(extension->fileAttributesSlot);
                const auto* name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + entry);
                Require(entry && entry < imageSize && std::strcmp(reinterpret_cast<const char*>(name->Name), "GetFileAttributesExW") == 0,
                        "the import slot does not name GetFileAttributesExW");
            }
            Require(extension->texImage2DTrampolineSlot && writable(extension->texImage2DTrampolineSlot),
                    "the trampoline slot is not in writable data");
            Require(extension->monsterTextureCount == 357, "the monster table has " + std::to_string(extension->monsterTextureCount) + " rows, not 357");
            if (extension->monsterTextureTable && extension->monsterTextureCount) {
                const auto* rows = static_cast<const uint8_t*>(extension->monsterTextureTable);
                const auto first = *reinterpret_cast<const char* const*>(rows), last = *reinterpret_cast<const char* const*>(rows + 32 * (extension->monsterTextureCount - 1));
                Require(inside(first) && std::strcmp(first, "mag005") == 0 && inside(last) && last[0],
                        "the monster table rows are not 32 bytes with a file name first");
            }
            // The format table: one row of 24 bytes per bimg format (85), RGBA8 (59) as GL_RGBA8 / GL_RGBA / unsigned bytes.
            Require(extension->textureFormatCount == 85, "the format table has " + std::to_string(extension->textureFormatCount) + " rows, not 85");
            if (extension->textureFormatTable && extension->textureFormatCount > 59) {
                const auto* rgba8 = static_cast<const uint32_t*>(extension->textureFormatTable) + 59 * 6;
                Require(rgba8[0] == 0x8058 && rgba8[2] == 0x1908 && rgba8[4] == 0x1401, "the format table's RGBA8 row is not GL_RGBA8/GL_RGBA/unsigned byte");
            }
            // No content decided in this mapping (no DllMain): the standard path, which drops no sub-image.
            if (extension->subImageDropped)
                Require(extension->subImageDropped() == 0, "a sub-image was dropped before any content was decided");
            Require(get() == extension, "a second call returned another structure");
        }
    }
    FreeLibrary(module);
    if (failures) return 1;
    std::printf("Demaster extension API: fields, import slot, trampoline slot and monster table passed\n");
    return 0;
}
