// ============================================================
// FILL AND EXPORT THE EXTENSION API (demaster_extension.h)
// Every field is an address of this DLL: nothing here runs the Demaster or needs DllMain, so the export may be
// called in an image mapped without initialization (tests/demaster_extension_tests.cpp does).
// ============================================================
#include "coreHeader.h"
#include "texture.h"
#include "config.h"
#include "image.h"
#include "demaster_extension.h"
#include <cstring>
#include <cstdint>

extern "C" IMAGE_DOS_HEADER __ImageBase;
// texture.cpp builds it under XXH_INLINE_ALL, whose 128-bit result type has another name: only the address is taken.
extern "C" void* __cdecl DemasterTextureHash(void* out, const void* data, size_t length, uint64_t seed);
const void* MonsterTextureTable(uint32_t& count);
DWORD* GetAtlasResolutionGfs(uint32_t battleId, int32_t a2, DWORD* width, DWORD* height);
void __cdecl SetWorldTextureObserver(DemasterWorldTextureObserver observer);
void __cdecl SetReplacementUpload(DemasterReplacementUpload upload);
void __cdecl SetUploadObserver(DemasterUploadObserver observer);
extern "C" int __cdecl SubImageDropped();

namespace {
// The import slot of `function` from `library` in this DLL's own import table; nullptr when it imports none.
void** ImportSlot(const char* library, const char* function)
{
	auto* const base = reinterpret_cast<uint8_t*>(&__ImageBase);
	const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + __ImageBase.e_lfanew);
	const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!directory.VirtualAddress)
		return nullptr;
	for (auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); descriptor->Name; ++descriptor)
	{
		if (_stricmp(reinterpret_cast<const char*>(base + descriptor->Name), library) != 0)
			continue;
		auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + (descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk));
		auto* slots = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + descriptor->FirstThunk);
		for (; names->u1.AddressOfData; ++names, ++slots)
		{
			if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
				continue;
			const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
			if (std::strcmp(reinterpret_cast<const char*>(import->Name), function) == 0)
				return reinterpret_cast<void**>(&slots->u1.Function);
		}
	}
	return nullptr;
}
DemasterExtensionV1 extension{};
}

extern "C" __declspec(dllexport) const DemasterExtensionV1* __cdecl DemasterGetExtensionV1()
{
	DemasterExtensionV1 filled{};
	filled.size = sizeof(DemasterExtensionV1);
	filled.version = 1;
	filled.contentProfile = CONTENT_DECIDED ? (LUNARCRY4_CONTENT ? 1 : 0) : -1;
	filled.loadImageFromFile = reinterpret_cast<void*>(static_cast<SafeBimg(*)(const char*)>(&LoadImageFromFile));
	filled.createImageRgba8 = reinterpret_cast<void*>(static_cast<SafeBimg(*)(uint32_t, uint32_t, const void*)>(&CreateImageRgba8));
	filled.textureHash = reinterpret_cast<void*>(&DemasterTextureHash);
	filled.fileAttributesSlot = ImportSlot("KERNEL32.dll", "GetFileAttributesExW");
	filled.texImage2DTrampolineSlot = reinterpret_cast<void**>(&ogl_tex_image2d);
	filled.hookGlTexImage2D = reinterpret_cast<void*>(&HookGlTexImage2D);
	filled.monsterTextureTable = MonsterTextureTable(filled.monsterTextureCount);
	filled.getAtlasResolutionGfs = reinterpret_cast<void*>(&GetAtlasResolutionGfs);
	filled.setWorldTextureObserver = &SetWorldTextureObserver;
	filled.setReplacementUpload = &SetReplacementUpload;
	filled.setUploadObserver = &SetUploadObserver;
	filled.subImageDropped = &SubImageDropped;
	filled.imageGetRawData = reinterpret_cast<void*>(static_cast<bool(*)(const bimg::ImageContainer&, uint16_t, uint8_t,
		const void*, uint32_t, bimg::ImageMip&)>(&bimg::imageGetRawData));
	static_assert(sizeof(TextureFormatInfo) == 24, "the format rows the API describes are 24 bytes");
	filled.textureFormatTable = s_textureFormat;
	filled.textureFormatCount = static_cast<uint32_t>(sizeof(s_textureFormat) / sizeof(s_textureFormat[0]));
	extension = filled;
	return &extension;
}
