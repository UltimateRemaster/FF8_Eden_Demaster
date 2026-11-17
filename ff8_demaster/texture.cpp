// ReSharper disable CppUseStructuredBinding
#include "coreHeader.h"
#include "texture.h"
#include "image_header.h"
#include <chrono>
#include <filesystem>
#define XXH_INLINE_ALL
#include <fstream>
#include <regex>
#include <xxhash.h>

#include "config.h"
#include "opengl.h"
#include "string_helper.h"
#include "demaster_extension.h"


std::map<GLuint, CapturedTextureInfo> g_capturedTextures;

void* __stdcall HookGlBindTexture(GLenum target, GLuint texture)
{
	return static_cast<void* (__stdcall*)(GLenum, GLuint)>(ogl_bind_texture)(target, texture);
}

//TODO - this is to handle params lol
void* __stdcall HookGlTextParameteri(GLenum target, GLenum name, GLint param)
{
	// Commenting because forcing filtering in whole game is not good

	/*
	if(target==GL_TEXTURE_2D &&
		(name == GL_TEXTURE_MIN_FILTER || name == GL_TEXTURE_MAG_FILTER))
	{
		return ((void* (__stdcall*)(GLenum, GLenum, GLint))oglTexParametri)
			(target, name, GL_NEAREST);
	}
	else
	*/
	return static_cast<void* (__stdcall*)(GLenum, GLenum, GLint)>(ogl_tex_parametri)
		(target, name, param);
}

struct loadedTextureInformation
{
	void* data;
	uint32_t length, width, height;
};

std::map<uint64_t, loadedTextureInformation> loadedTextures;

// ============================================================
// THE ONE HASH OF THE TEXTURE HASHING, AT A FIXED ENTRY
// TextureHashingSaveLoad calls only this, a function of its own in every build (demaster_extension.h, textureHash).
// ============================================================
extern "C" __declspec(noinline) XXH128_hash_t* __cdecl DemasterTextureHash(XXH128_hash_t* out, const void* data,
	size_t length, uint64_t seed)
{
	*out = XXH128(data, length, seed);
	return out;
}

// ============================================================
// HOOK POINTS OF THE EXTENSION API (demaster_extension.h)
// The uploads the Demaster sends itself through the glTexImage2D trampoline go around every hook of the OPENGL32
// entry: an observer may be told of them. The upload of a decoded hashOutput replacement may be taken over.
// ============================================================
static DemasterReplacementUpload replacementUpload = nullptr;
static DemasterUploadObserver uploadObserver = nullptr;
void __cdecl SetReplacementUpload(DemasterReplacementUpload upload) { replacementUpload = upload; }
void __cdecl SetUploadObserver(DemasterUploadObserver observer) { uploadObserver = observer; }
static void* TrampolineUpload(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const void* pixels)
{
	void* result = static_cast<void* (__stdcall*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
		const void*)>(ogl_tex_image2d)(target, level, internalformat, width, height, border, format, type, pixels);
	if (const auto observer = uploadObserver)
		observer(target, level, internalformat, width, height, border, format, type, pixels);
	return result;
}
static void* ReplacementUpload(uint64_t key, const bimg::ImageContainer* image, GLenum target, GLint level,
	GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
{
	if (const auto upload = replacementUpload)
		return upload(key, image, target, level, internalformat, width, height, border, format, type, pixels);
	return TrampolineUpload(target, level, internalformat, width, height, border, format, type, pixels);
}

void* TextureHashingSaveLoad(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data, GLuint boundId, int lengthModifier, bool& returns)
{
	returns = false;
	if(HASH_ENABLED) //=======HASHING====//
	{
		// Only level 0: the engine sends every texture as level 0 (its 19 glTexImage2D calls in the EFIGS all pass level
		// 0); a further level here is the Demaster's own upload of an image's mip chain (image.cpp), which now carries
		// the level's own size and would otherwise be hashed and could be replaced as a texture of its own.
		if (level == 0 && data != nullptr && width != 0 && height != 0 && lengthModifier != 0
			&& width < 1024 && height < 1024)
		{
			uint64_t hashCopy = 0;
			const std::chrono::time_point<std::chrono::steady_clock> start =
				std::chrono::high_resolution_clock::now();
			//XXH32_hash_t hash = XXH32(data, width * height * lengthModifier, 0x85EBCA77U); //32ms
			int hashLength = width*height*lengthModifier;
			if(currentMode == Modes::MODE_WORLDMAP)
				if(width==256 && height == 256)
					hashLength = 256*192*lengthModifier;
			XXH128_hash_t hashed{};
			DemasterTextureHash(&hashed, data, static_cast<size_t>(hashLength), 0x85EBCA77U);
			const auto low64 = hashed.low64, high64 = hashed.high64;
			hashCopy = high64;
			std::filesystem::path destinationPath = std::filesystem::current_path();
			destinationPath.append(std::string(DIRECT_IO_EXPORT_DIR)
				+ "hashOutput");
			if (!std::filesystem::exists(destinationPath))
				std::filesystem::create_directory(destinationPath);
			std::stringstream ss;
			ss << std::setw(16) << std::setfill('0') << std::hex
				<< std::uppercase << high64 << std::dec << std::nouppercase;
			destinationPath.append(ss.str());
			if (!knownTextures.contains(high64))
			{
				OutputDebug("New hash for textureId: %u. Hash: %016llX%016llX ",
				            boundId, high64, low64);
				knownTextures.insert(std::pair(high64, TexImageInformation{
					                               low64, static_cast<GLuint>(boundId), internalformat, width, height
				                               }));
				if (HASH_OUTPUT) //======OUTPUT OF HASHED TEXTURES======//
				{
					std::string exportPath = std::string(destinationPath.string());
					exportPath.append(GetHashExtension(true));
					if (!std::filesystem::exists(exportPath))
					{
						bx::FileWriter writer;
						bimg::TextureFormat::Enum fmt = bimg::TextureFormat::RGBA8;
						switch (internalformat)
						{
						case GL_RGB8:
						case GL_RGB:
						case GL_BGR: fmt = bimg::TextureFormat::BGRA8;
							break;
						case GL_BGRA: fmt = bimg::TextureFormat::BGRA8;
							break;
						case GL_RGBA: default: fmt = bimg::TextureFormat::BGRA8;
							break;
						}
						if (bx::open(&writer, bx::FilePath(exportPath.c_str())))
						{
							//refractor recommends switching to if/else for binary switch, however it's left here for future
							//if more formats are added
							/*switch(HASH_OUTPUT_EXT)
					   {
						   default:
							   OutputDebug("Only PNG is supported for now. Saving as PNG");
							   [[fallthrough]]
						   case 0:
							   bimg::imageWritePng(&writer, width, height, width * lengthModifier
						   , data, fmt, false);
						   break;
					   }*/
							bimg::imageWritePng(&writer, width, height, width * lengthModifier
							                    , data, fmt, false);
						}
					}
				}
			}
			if(HASH_LOAD_HD) //=====HASH LOAD CODE=====//
			{
				std::string importPath = std::string(destinationPath.string());
				importPath.append(HASH_HD_SUFFIX);
				importPath.append(GetHashExtension(false));
				if (std::filesystem::exists(importPath))
				{
					if (loadedTextures.contains(hashCopy))
					{
						const loadedTextureInformation& loaded = loadedTextures[hashCopy];
						returns = true;
						return ReplacementUpload(hashCopy, nullptr, target, level, internalformat,
							static_cast<GLsizei>(loaded.width), static_cast<GLsizei>(loaded.height), border, format, type, loaded.data);
					}
					//Obtain file resolution
					if(Vector2Di resolution = GetImageResolutionFast(importPath.c_str());
						resolution.width != static_cast<uint32_t>(width)
						&& resolution.height != static_cast<uint32_t>(height)
						&& (resolution.width != 0 && resolution.height != 0))
					{
						SafeBimg img = LoadImageFromFile(importPath.c_str());
						if (const bimg::ImageContainer* imageContainer = img.get();
							imageContainer->m_data != nullptr)
						{
							OutputDebug("Loading custom HD texture: %s!", destinationPath.string().c_str());

						
							if(bimg::isCompressed(imageContainer->m_format))
							{
								RenderTexture(imageContainer); //redundant on checking two times for compression, but whatev
								// The upstream then called glTexImage2D with level -1, which only raised GL_INVALID_VALUE
								// for the next glGetError of whoever asks: no call.
								returns = true;
								return nullptr;
							}
							loadedTextureInformation lti{};
							lti.height = imageContainer->m_height;
							lti.width = imageContainer->m_width;
							lti.length = imageContainer->m_size;
							lti.data = malloc(lti.length);
							memcpy(lti.data, imageContainer->m_data, lti.length);
							//add lti to loadedTextures
							loadedTextures.emplace(hashCopy, lti);
							returns = true;
							return ReplacementUpload(hashCopy, imageContainer, target, level, internalformat,
								static_cast<int>(imageContainer->m_width), static_cast<int>(imageContainer->m_height), border,
								GL_RGBA/*format*/, type, imageContainer->m_data);
						}
					}
				}
			}
		}
	}
}

//below could use splitting into multiple functions
void* __stdcall HookGlTexImage2D(GLenum target,
	GLint level,
	GLint internalformat,
	GLsizei width,
	GLsizei height,
	GLint border,
	GLenum format,
	GLenum type,
	const void* data)
{
	GLuint boundId = GetCurrentBoundTextureID();
	if (DEBUG_GLTEXIMAGE2D)
		OutputDebug("glTexImage2D (%d): format: %d, %dx%d, void: %08x\n",boundId, internalformat, width, height, data);
	int lengthModifier = 0;
	if (internalformat == GL_RGBA || internalformat == GL_BGRA
		|| internalformat == GL_RGBA8)
		lengthModifier = 4;
	else if (internalformat == GL_RGB || internalformat == GL_BGR
		|| internalformat == GL_RGB8)
		lengthModifier = 3;


	
	// === HASHING AND LOADING TEXTURES === //
	bool returns;
	void* value1 = TextureHashingSaveLoad(target, level, internalformat, width, height, border, format, type, data, boundId,
	                                      lengthModifier, returns);
	if (returns) return value1;

	

	// We only want to capture the main texture data (level 0) for 2D textures
	if (target == GL_TEXTURE_2D && level == 0)
	{
		if (boundId != 0 && width > 0 && height > 0)
		{
			// Add or update the texture in our map.
			g_capturedTextures[boundId] = {.id = boundId, .width = width, .height = height, .internalFormat = internalformat,
			.estimatedFilename = LastFilePath};
		}
	}
	
	//serverInst.WriteLog(std::format("GLTexImage2D: ID: {} Width: {} Height: {}", boundId, width, height));
	return static_cast<void* (__stdcall*)(GLenum, GLint, GLint, GLsizei, GLsizei,
	                                      GLint, GLenum, GLenum, const void*)>(ogl_tex_image2d)
		(target, level, internalformat, width, height
			, border, format, type, data);
}

// ============================================================
// WHAT THE SUB-IMAGE PATH DOES WITH THE FILE THE ENGINE OPENED LAST
// Standard content (Demaster 1.3.7) keeps its art in the .png the engine sent: every sub-image goes on as it came.
// Lunar Cry 4 content (d2f6320, content_profile.h) packs MAG and c0m121/122/126 as one .dds per even chunk: an even
// chunk is replaced by that art, an odd chunk and every c0m123 are dropped (no GL call at all), anything else goes on.
// ============================================================
enum class SubImageAction { Pass, Drop, Replace };
static SubImageAction DecideSubImage(const std::string& path)
{
	if (!LUNARCRY4_CONTENT)
		return SubImageAction::Pass;
	if (Contains(path, "mag")) //MAG files
	{
		static const std::regex chunkPattern("_(\\d+)\\."); // "_0." and etc.
		std::smatch match;
		if (std::regex_search(path, match, chunkPattern))
			return std::stoi(match[1].str()) % 2 != 0 ? SubImageAction::Drop : SubImageAction::Replace;
		return SubImageAction::Pass;
	}
	if (Contains(path, "c0m")) //Monster files
	{
		static const std::regex monsterPattern("c0m(\\d+)_(\\d+)");
		std::smatch match;
		if (std::regex_search(path, match, monsterPattern))
		{
			const int monsterIndex = std::stoi(match[1].str()); //c0mXXX_Y.ZZZ -> X
			const int textureIndex = std::stoi(match[2].str()); //c0mXXX_Y.ZZZ -> Y
			if (monsterIndex == 121 || monsterIndex == 122 || monsterIndex == 126) //LastBattle
				return textureIndex % 2 != 0 ? SubImageAction::Drop : SubImageAction::Replace;
			if (monsterIndex == 123) //Skip Griever balls. c0m123_0 should be on the bottom of _4 textures for 121 and 122
				return SubImageAction::Drop;
		}
	}
	return SubImageAction::Pass;
}
// 1 when the glTexSubImage2D the engine sends now, with the file it opened last, is dropped: it reaches no GL call
// (demaster_extension.h, subImageDropped).
extern "C" int __cdecl SubImageDropped()
{
	return DecideSubImage(LastFilePath) == SubImageAction::Drop ? 1 : 0;
}

void __stdcall HookGlTexSubImage2D( 	GLenum target,
	  GLint level,
	  GLint xoffset,
	  GLint yoffset,
	  GLsizei width,
	  GLsizei height,
	  GLenum format,
	  GLenum type,
	  const void * pixels)
{
	OutputDebug("GlTexSubImage2D: ID: %d Width: %d Height: %d. Last path: %s\n", GetCurrentBoundTextureID(),
		width, height, LastFilePath.c_str());

	const SubImageAction action = DecideSubImage(LastFilePath);
	if (action == SubImageAction::Drop)
	{
		OutputDebug("Skipping chunk %s\n", LastFilePath.c_str());
		return;
	}
	const bool tryToReplace = action == SubImageAction::Replace;

	if (tryToReplace)
	{
		std::string localTexturePath = LastFilePath;
		localTexturePath.replace(localTexturePath.end() - 4, localTexturePath.end(), ".dds");
		if (std::filesystem::exists(localTexturePath))
		{
			OutputDebug("GlTexSubImage2D: Found DDS. Using DDS for %s\n", localTexturePath.c_str());
			LoadAndRenderTexture(localTexturePath.c_str(), false);
			return;
		}
		const Vector2Di resolution = GetImageResolutionFast(LastFilePath.c_str());
		TrampolineUpload(target, level, format, resolution.width, resolution.height, 0, format, type, pixels);
		return;
	}
	
	OutputDebug("Hooked glTexSubImage2D width: %d height: %d\n", width, height);
	
	static_cast<void (__stdcall*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei,
									  GLenum, GLenum, const void*)>(ogl_subTexImage2D)
	(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void* __stdcall HookGlTextureSubImage2D( 	GLuint texture,
	  GLint level,
	  GLint xoffset,
	  GLint yoffset,
	  GLsizei width,
	  GLsizei height,
	  GLenum format,
	  GLenum type,
	  const void *pixels)
{
	OutputDebug("Hooked glTextureSubImage2D width: %d height: %d\n", width, height);
	return static_cast<void* (__stdcall*)(GLuint, GLint, GLint, GLint, GLsizei, GLsizei,
								  GLenum, GLenum, const void*)>(ogl_subTexImage2D)
(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}

//this is null sub- basically hooking api requires static cast to function pointer. However if there's need to
//actually call compressed texture function, this is the way to do it. I call the compressed texture method and
//then return the main texture2D method into this null function. The compressed texture and glTexImage2D are voids
//that don't return anything as they work under bindings, so compressed texture will utilize the bound texture
//while this null reroute will do nothing and not overwrite the texture
void NullHookGlTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
	GLenum format, GLenum type, const void* data)
{
}


BYTE* cltBackAdd1;
BYTE* cltBackAdd2;
DWORD* _thisFF8 = nullptr; //__thiscall, so this is ECX



const char* GetTextureType(const int texType)
{
	static char textureType[4]{0};
	switch (texType)
	{
	case 11:
		return "BATTLE";
	case 18:
		return "WORLDMAP";
	case 25:
		return "FIELDBG";
	case 35:
		return "BATTLECHARACTER";
	case 57:
		return "FIELDENTITY";
	default:
		sprintf(textureType, "%02d", texType % 100);
		return textureType;
	}
}

__declspec(naked) void _cltObtainTexHeader()
{
	__asm
	{
		MOV tex_header, ESI
		MOV gl_textures, EDI //gl_textures is actually tex_struct

		//ORIGINAL CODE
		PUSH ECX
		PUSH EDX //this one gets cleared after 1020
		PUSH UPLOADVRAM
		CALL GetAddress

		MOV ECX, EAX
		MOV EAX, OFFSET IMAGE_BASE
		MOV EAX, [EAX]
		ADD EAX, ECX //upload_texture_vram
		POP EDX
		POP ECX
		CALL EAX
			
		JMP cltBackAdd2
	}
}

void _cltVoid()
{
	const int textureType = tex_struct[48];
	TEX_TYPE = textureType;
	const int palette = tex_header[52];
	const UINT unknownDword = tex_struct[65];
	const BOOL bHaveHD = tex_struct[49];
	const DWORD tPage = tex_struct[50];



	char n[255];
	n[0] = '\0';
	if (textureType == 0)
		return;
	if (textureType == 18 && (tPage <= 15 || tPage>=28))
		return;
	if (textureType == 57) //field
		return;
	//OutputDebug("\ncommon_load_texture: tex_type: %s, pal: %d, unk: %08x, bHaveHD: %s, Tpage: %d\n", GetTextureType(textureType), palette, unknownDword, bHaveHD > 0 ? "TRUE" : "FALSE", tPage);

	return;
}

__declspec(naked) void _cltObtainTexStructDebug()
{
	__asm
	{
		PUSH EAX
		PUSH EBX
		PUSH ECX
		PUSH EDX
		MOV tex_struct, EDX
		CALL _cltVoid

		POP EDX
		POP ECX
		POP EBX
		POP EAX

		//ORIGINAL CODE

		CMP dword ptr[edx + 0x0BC], 0
		JMP cltBackAdd1
	}
}

// The size from the header (image_header.h: a DDS's dwWidth and dwHeight in their own places; the upstream reader had
// them swapped). lastGetImageResolution keeps the size as the atlas hooks have always seen it (AtlasSize): the Shiva
// atlas reads it.
Vector2Di GetImageResolutionFast(const char* filePath)
{
	const ImageHeaderSize size = ReadImageHeaderSize(filePath);
	const Vector2Di resolution{size.width, size.height};
	if (!size.width || !size.height)
		OutputDebug("%s error. %s is missing or neither a PNG nor a DDS\n", __func__, filePath);
	OutputDebug("%s: %s resolution: %dx%d\n", __func__, filePath, resolution.width, resolution.height);
	const ImageHeaderSize seen = AtlasSize(size);
	lastGetImageResolution = Vector2Di{seen.width, seen.height};
	return resolution;
}

// The side a battle atlas is sized from, as it always was (image_header.h, AtlasSide).
DWORD GetImageAtlasSideFast(const char* filePath)
{
	const ImageHeaderSize size = ReadImageHeaderSize(filePath);
	const ImageHeaderSize seen = AtlasSize(size);
	lastGetImageResolution = Vector2Di{seen.width, seen.height};
	return seen.width;
}

void ReplaceTextureFunction()
{
	OutputDebug("Applying texture patches...\n");
	if (BATTLE_CHARA)
	{
		OutputDebug("Applying BATTLE_CHARA patch\n");
		ApplyBattleCharacterPatch();
	}
	if (FIELD_ENTITY)
	{
		OutputDebug("Applying FIELD_ENTITY patch\n");
		ApplyFieldEntityPatch();
	}
	if (BATTLE_HOOK)
	{
		OutputDebug("BATTLE_HOOK- STARTING PATCHING\n");
		ApplyBattleHookPatch();
		ApplyBattleMonsterPatch();
		ApplyBattleFieldPatch();
	}
	if (FIELD_BACKGROUND)
	{
		OutputDebug("Applying FIELD_BACKGROUND PATCH\n");
		ApplyFieldBackgroundPatch();
	}
	if (WORLD_TEXTURES)
	{
		OutputDebug("Applying WORLD_TEXTURES PATCH\n");
		ApplyWorldPatch();
	}

	cltBackAdd2 = InjectJMP(IMAGE_BASE + GetAddress(CLTBACKADD1), reinterpret_cast<DWORD>(_cltObtainTexHeader), 5);
	cltBackAdd1 = InjectJMP(IMAGE_BASE + GetAddress(CLTBACKADD2), reinterpret_cast<DWORD>(_cltObtainTexStructDebug), 7);
}
