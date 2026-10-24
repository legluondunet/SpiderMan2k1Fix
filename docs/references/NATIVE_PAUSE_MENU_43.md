# Native gameplay ESC menu: 4:3 presentation in widescreen

The original **Spider-Man PC (2001)** 3D gameplay ESC screen has a
full-screen translucent orange/blue geometric motif, a purple options ribbon,
text, and a red spider-shaped selector. These are 2D Direct3D7 primitives
drawn over the 3D world. In 1920×1080 Auto mode the original 4:3 artwork
was stretched horizontally.

The corrections were **visually validated under Wine on 2026-10-24**:
the ribbon, its outline and labels, translucent motif, background violet tint
(including both screen edges), and cursor keep their intended proportions.
Both the main ESC choices and the **REALLY QUIT? → NO / YES** dialog were
tested. The normal gameplay HUD, compass and 3D scene are unaffected.

## Activation / supported scope

The player INI no longer exposes a HUD switch. Widescreen HUD and the three
native ESC 4:3 corrections are **permanently requested**, subject to the
verified executable, the game stage and the successful full native HUD patch.
The original `4:3` gameplay presentation retains its original geometry; the
widescreen transform is not needed in that case.

Historical development toggles `HUDWidescreenFix`, `NativeESC4x3`,
`NativeMotif4x3`, and `NativeCursor4x3` are accepted only so existing INIs
can be migrated safely; **they no longer disable these features**. The old
`SPIDERMAN2K1FIX_NATIVE_ESC_43`, `SPIDERMAN2K1FIX_NATIVE_MOTIF_43`, and
`SPIDERMAN2K1FIX_NATIVE_CURSOR_43` environment overrides are likewise inert.
The code still refuses unsafe changes if the native HUD hook or original
D3D7 geometry signatures do not match.

The motif and selector signatures were visually calibrated and confirmed at
**1920×1080 Auto** (borderless), not at every display resolution. They remain
disabled in main menus and in classic 4:3 gameplay. Other resolutions and
native graphics drivers still require coverage.

## Implementation details / safety

The hooks edit **copies of D3D7 screen-space vertices**, never engine-owned
vertex data or input state. Native `DrawPrimitive` and exact owner RVA
`0x00102C72`, FVF `0x144`, 4-vertex triangle fans, texture dimensions,
Z/RHW and current render mode distinguish the ESC UI from the world and HUD.

- **Ribbon:** Match the wide untextured pause band, then consider only
  exact 8x8 border textures and text glyphs in the *same frame*.
  Empirical outline calibration gives the original 4:3 width
  (approximately 1440px at 1920×1080).
- **Motif:** Match *untextured* 2D alpha-0x80 primitives at Z≈0.00146,
  RHW≈9.988. Scale X around display center by 0.75, preserving Y.
  The original artwork remains in 4:3 without touching world triangles.
- **Violet tint:** Resizing motif primitives also narrowed the game's native
  full-screen darkening. To preserve the original screen-wide transparency,
  replay the original triangle-fan geometry only in the two outer strips,
  x=0..240 and x=1680..1920. Each triangle is clipped, interpolating
  Z, RHW, diffuse colors and UV; use the game's original blend state.
  No duplicate tint is drawn over the central 4:3 region.
- **Red selector:** Its native textured quad is **96×72**, from a 32×32
  texture, at Z≈0 and RHW≈27.519. Scale only X by 0.75 **around its own
  center** to achieve 72×72, without position limits. The original
  restrictions on top/right caused intermittent 96px-wide cursors when
  moving outside the central menu area; this was fixed and Wine-validated.

Diagnostics from research builds were intentionally removed before merging.
When troubleshooting, enable `[Debug] Log=1` to obtain limited native
ESC matching and gutter replay records; F9 captures PNG screenshots.

## Regression checks

Check in Auto 1920×1080:

1. Normal 3D play shows no overlay; the HUD and compass retain their 4:3
   proportions and widescreen world rendering.
2. Open **ESC**: violet darkening reaches both physical screen edges, orange
   motif and purple panel are centered and not horizontally stretched.
3. Move the red icon near the center, the top of the screen, and near the
   right edge: width and height keep the same ratio everywhere.
4. Confirm **QUIT → REALLY QUIT?**: the **NO / YES** dialog, banner,
   motif and selector retain their proportions.
5. Return to the main menu and try 4:3 gameplay mode: neither should be
   affected by the experimental native ESC Auto fix.

Automated source and clipping tests live in
`tests/test_native_pause_menu_probe.py`. The final F9 Wine visual checks
were performed by the project maintainer.

## Credits

legluondunet assisted by ChatGPT (OpenAI)
