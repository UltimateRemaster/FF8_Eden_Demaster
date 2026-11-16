#pragma once

#define DEMASTER_CONF "demaster.conf"

inline int AUTOUNPACK;
inline int UVPATCH;
inline int DIRECT_IO;
inline int TEXTURE_PATCH;
inline int DEBUG_PATCH;
inline int IMGUI_DEBUG;
inline int LOG;
inline int BATTLE_CHARA;
inline int FIELD_ENTITY;
inline int BATTLE_HOOK;
inline int FIELD_BACKGROUND;
inline int WORLD_TEXTURES;
inline int LINEAR_PATCH;
inline int OPENGL_HOOK;
inline int FILL_ASPECT_RATIO;
inline int BATTLE_STAGE_FORCE_RELOAD;
inline int HASH_ENABLED;
inline int HASH_OUTPUT;
inline int HASH_LOAD_HD;
inline int SKIP_SPLASH;

inline int BATTLE_STAGE_ANIMATION_DELAY;
inline int HASH_OUTPUT_EXT;
inline int HASH_LOAD_HD_EXT;

inline int WINDOW_OVERWRITE;

inline int DEBUG_GLTEXIMAGE2D;

// -1 (default): the installed content decides (content_profile.h); 0: standard Demaster 1.3.7 content; 1: Lunar Cry 4
// or later.
inline int CONTENT_PROFILE = -1;   // also without a demaster.conf (ReadConfigFile returns before reading it)
// What the content was made for, decided once at start: 1 Lunar Cry 4 or later, 0 standard. CONTENT_DECIDED says
// the decision was made (DllMain), for the interface's contentProfile.
inline int LUNARCRY4_CONTENT;
inline int CONTENT_DECIDED;

void ReadConfigFile();