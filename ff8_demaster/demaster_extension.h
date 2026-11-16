#pragma once
// ============================================================
// DEMASTER EXTENSION API, VERSION 1 (ABI, X86)
//
// Addons that work beside the Demaster used to find its functions and data at fixed addresses of one build. This
// build hands them over by one export instead, together with a few places where an addon may hook in:
//
//   const DemasterExtensionV1* __cdecl DemasterGetExtensionV1();
//
// An addon keeps a copy of this structure. Its layout is a contract: fields are only ever appended, `size` says
// how many a build has, `version` stays 1 for this layout. Plain pointers, no C++ types.
// The export fills the structure on every call and needs no DllMain: it may be called in an image mapped without
// initialization (contentProfile then says -1).
// ============================================================
#include <cstdint>

extern "C" {
// Called instead of the upload of a decoded hashOutput replacement (texture.cpp, TextureHashingSaveLoad), with the
// key of the replacement (high 64 bits of its texture hash), the decoded image (bimg::ImageContainer*) on the first
// load or nullptr on a cache hit, and the glTexImage2D arguments the Demaster would send through the trampoline
// (texImage2DTrampolineSlot). The hook sends whatever it decides, and tells any observer of its own.
typedef void* (__cdecl* DemasterReplacementUpload)(uint64_t key, const void* image, uint32_t target, int32_t level,
    int32_t internalFormat, int32_t width, int32_t height, int32_t border, uint32_t format, uint32_t type, const void* pixels);
// Told after every glTexImage2D the Demaster sends itself through the trampoline, around every hook of the
// OPENGL32 entry, with the arguments it sent.
typedef void (__cdecl* DemasterUploadObserver)(uint32_t target, int32_t level, int32_t internalFormat, int32_t width,
    int32_t height, int32_t border, uint32_t format, uint32_t type, const void* pixels);
// Told with the file name after every LoadImageIntoWorldStruct that returns true for a slot below 35.
typedef void (__cdecl* DemasterWorldTextureObserver)(const char* file);

struct DemasterExtensionV1 {
    uint32_t size;                    // sizeof(DemasterExtensionV1) of the build that filled it
    uint32_t version;                 // 1
    int32_t contentProfile;           // 0: content for the standard Demaster 1.3.7; 1: Lunar Cry 4 or later; -1: not decided
    uint32_t reserved;                // 0

    // ---- images ----
    // SafeBimg __cdecl LoadImageFromFile(const char* file): returned through hidden storage {deleter, image}.
    void* loadImageFromFile;
    // SafeBimg __cdecl CreateImageRgba8(uint32_t width, uint32_t height, const void* rgba8): an RGBA8 image (bimg
    // format 59) in the Demaster's own bimg allocator, freed by the owner's deleter; returned like loadImageFromFile.
    void* createImageRgba8;
    // XXH128_hash_t* __cdecl TextureHash(XXH128_hash_t* out, const void* data, size_t bytes, uint64_t seed): the only
    // hash the texture hashing calls (seed 0x85EBCA77), a function of its own in every build.
    void* textureHash;
    // The Demaster's import slot of KERNEL32!GetFileAttributesExW (its file existence checks).
    void** fileAttributesSlot;
    // The slot that holds the MinHook trampoline of OPENGL32!glTexImage2D, and the entry of the Demaster's own
    // glTexImage2D hook.
    void** texImage2DTrampolineSlot;
    void* hookGlTexImage2D;

    // ---- battle ----
    // TexFuncMonsterTexProvider: rows of 32 bytes (file name pointer first), the table the engine is pointed at.
    const void* monsterTextureTable;
    uint32_t monsterTextureCount;
    // The detour installed on the engine's battle atlas size query (BHP_ATLAS).
    void* getAtlasResolutionGfs;

    // ---- hook points (null removes) ----
    void(__cdecl* setWorldTextureObserver)(DemasterWorldTextureObserver observer);
    void(__cdecl* setReplacementUpload)(DemasterReplacementUpload upload);
    void(__cdecl* setUploadObserver)(DemasterUploadObserver observer);

    // ---- added after the first layout (present when size reaches it) ----
    // 1 when the glTexSubImage2D the engine sends now, with the file it opened last, is dropped by the Demaster: no
    // GL call at all, the bound texture keeps what it had. Ask right before the call reaches the Demaster's hook.
    int(__cdecl* subImageDropped)(void);
    // bool __cdecl ImageGetRawData(const bimg::ImageContainer& image, uint16_t side, uint8_t level, const void* data,
    // uint32_t size, bimg::ImageMip& mip): the bimg reader of one level of a decoded image (the `image` a
    // replacement-upload hook is handed).
    void* imageGetRawData;
    // The GL format of every bimg format, indexed by bimg::TextureFormat: rows of 24 bytes {internal format, internal
    // sRGB format, format, sRGB format, type, supported}.
    const void* textureFormatTable;
    uint32_t textureFormatCount;
};
}
