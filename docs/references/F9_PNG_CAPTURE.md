# F9 lossless PNG screenshots

On the supported, SHA-256-verified `SpideyPC.exe`, the original **F9**
key now saves full-resolution, lossless `scrnNNNN.png` files.
**PNG is the only selectable format: there is no INI setting, environment
variable or user option to switch to BMP.** Captures work with the default
`[Debug] Log=0`; diagnostic logging is optional.

The game still performs its own DirectInput F9 polling, sound and 500 ms
repeat delay. SpiderMan2k1Fix verifies the two original x86 screenshot
`CALL` instruction signatures (RVA `0x0010A1DC` and `0x0010A62B`)
and replaces those calls with a PNG encoder callback. The original BMP
writer is bypassed whenever the signature-checked hooks are installed.

**To capture a screenshot:** press **F9** during a video, menu, comic,
cinematic or gameplay. Files are written beside `SpideyPC.exe`.
After a successful save, the game displays a brief English confirmation
and the full PNG path, without baking that confirmation into the image.
The implementation captures decoded movie frames or DirectDraw
backbuffers as appropriate, immediately copies the pixels to private
memory, and encodes on a separate worker using Windows/Wine GDI+.
Filename collisions are avoided; save errors are shown and never
deliberately cause a BMP fallback.

**Safety:** for an unsupported EXE SHA-256, or if one of the original
CALL signatures cannot safely be patched, SpiderMan2k1Fix does not
force an unsafe rewrite. In that exceptional case the game's original
screenshot code is left untouched and may still produce BMPs. This is
a protection against unsupported binaries, **not a configurable format
choice**. Enable `Log=1` only to diagnose such failures.

**Validation:** the user verified the preceding opt-in PNG implementation
under Wine in PR #25, build #717: eleven 1920×1080 PNGs
(`scrn0000.png` through `scrn0010.png`) from intro movies,
menus, comics and 3D. The new *always-on* initialization still requires
a fresh Wine and native Windows 11 regression test.

The old `SPIDERMAN2K1FIX_F9_PNG_ONLY`, `SPIDERMAN2K1FIX_F9_PNG`
and `SPIDERMAN2K1FIX_F9_BACKBUFFER` variables no longer select any
capture format. The separate, read-only
`SPIDERMAN2K1FIX_F9_TRACE=1` diagnostic remains available when
`Log=1`; it does not change the image format. Historical research on
native Windows magenta BMPs is retained in closed PR #24.

For a deeper analysis of the original game's BMP failure, see [the reverse-engineering reference](../DEVELOPMENT.md#104-f9-bmp-purple-screenshots-why-simply-converting-the-bmp-was-insufficient).
