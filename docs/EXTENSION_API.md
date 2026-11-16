# Content profiles and the extension API

## One binary for Lunar Cry 4 content and for the content made before it

The Demaster that ships with Lunar Cry 4 (upstream `d2f6320`) and the standard Demaster (`1.3.7`, `0d8cf54`) differ in
four places, all about battle textures:

| Place | Lunar Cry 4 | Standard |
| --- | --- | --- |
| `HookGlTexSubImage2D` (`texture.cpp`) | replaces MAG and `c0m121/122/126` chunks by the even chunk's `.dds`, drops odd chunks and `c0m123` | passes the sub-image on |
| `_bhpVoid`, MAG (`texturepatch_v2_battleHooks.cpp`) | atlas width = width of `mag###_0` / 2 | full width |
| `_bhp` | skips the engine's replacement checks | goes through them |
| `GetAtlasResolutionGfs` | logs and returns | the 1.3.7 table |

This build runs either code, chosen once at start from the installed files (`content_profile.h`): an even chunk's `.dds`
(`magNNN_K.dds`, `c0m121|122|126_K.dds`) beside a 4×4 `.png` of the same name in
`DEMASTER_EXP\textures\battle.fs\hd_new` is Lunar Cry 4 content or later; anything else is standard content.
`CONTENT_PROFILE` in `demaster.conf` overrides it: `-1` (default) lets the files decide, `0` standard, `1` Lunar Cry 4.

## Fixes of upstream defects

- **Mip levels** (`upload_levels.h`, `image.cpp`): every level is sent with its own size, `max(1, W >> k)` by
  `max(1, H >> k)`. The upstream sent level 0's size for every level: GL refused the compressed levels past the first and
  made full-size levels of the uncompressed ones.
- **Texture hashing** (`texture.cpp`): only level-0 uploads are hashed (the engine sends only level 0; further levels
  are the Demaster's own mip chains); the compressed replacement no longer issues a `glTexImage2D` with level -1.

## Left out

The experimental widescreen of `0107523` (forced `SetResolution` and `glScissor`) and the draw-pointer hooks of
`9812ca2` (their captures were only read inside `#if 0`).

## Extension API

`demaster_extension.h`: `const DemasterExtensionV1* __cdecl DemasterGetExtensionV1()`, a versioned structure with the
addresses addons used to find at fixed offsets of one build (image loader and creator, the texture hash, the
`GetFileAttributesExW` import slot, the `glTexImage2D` trampoline slot and hook entry, the monster texture table, the
battle atlas detour) and three hook points: a World texture observer, a replacement-upload hook and an observer of the
uploads the Demaster sends through the trampoline. Fields are only appended; `size` tells how many a build has.

## Tests and build

- `tools\run_tests.cmd`: the pure logic (mip level sizes, packed chunk names, content profiles), x86, no GL.
- `tools\build_dll.cmd`: Debug|Win32 into `.build\eden\out`.
