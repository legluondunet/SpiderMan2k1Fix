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

These corrections now follow the existing **[HUD] HUDWidescreenFix** master
(which defaults to 1). They are ON by default for the supported
SHA-verified executable once the full HUD widescreen patch is installed.

- If `HUDWidescreenFix=0`, **all three native ESC corrections are OFF**,
  regardless of individual child settings. They are also disabled when the
  full HUD patch fails, or the original gameplay 4:3 aspect is selected.
  Passive HUD scopes-only mode (for FOV diagnostics) does not enable them.
- The hidden children `NativeESC4x3`, `NativeMotif4x3`, and
  `NativeCursor4x3` are read from the **[HUD]** section. Each defaults to 1,
  but none is included in any generated or distributed INI template.
- The banner/labels child `NativeESC4x3=0` is the native ESC sub-master
  and also disables the motif and cursor. The other two can be disabled
  individually, without changing the ribbon or text.
- The original 2026 experimental environment toggles are no longer required;
  an explicit `SPIDERMAN2K1FIX_NATIVE_ESC_43=0`,
  `SPIDERMAN2K1FIX_NATIVE_MOTIF_43=0`, or
  `SPIDERMAN2K1FIX_NATIVE_CURSOR_43=0` remains a legacy debug opt-out.
  None may force a child ON if its parent is disabled in the INI.

For example, to disable only the orange/blue motif while investigating
a graphics issue, add this key to your existing file:

```ini
[HUD]
NativeMotif4x3=0
```

To disable **all** HUD corrections, use `HUDWidescreenFix=0` instead.
Child keys are strict 0/1 values and recognized by INI validation and
automatic recovery, which preserve legitimate manual overrides.

The motif and selector signatures are validated for **1920×1080 Auto**
(borderless) only; they are disabled for the main menu and 4:3 gameplay mode.
The ESC ribbon has a broader aspect transform but its pixel calibration was
tested at 1920×1080. Other screen resolutions and native Windows graphics
drivers have **not** been validated for these specific corrections.

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
