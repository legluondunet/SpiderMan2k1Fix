# Native ESC pause menu: centered 4:3 diagnosis

## Desired visual behavior

Keep the widescreen 3D world and already validated gameplay HUD
(health, compass, instructions) unchanged. The native ESC pause UI
(red PAUSED title, purple ribbon, CONTINUE / RESTART LEVEL / QUIT
labels and red selection icon) should use the **same centered 4:3
reference** as the independently implemented Alt+Tab focus-pause panel.

At **1920×1080**, the 4:3 visual area is **1440×1080**, from
x=240 to x=1680, centered at x=960. The expected horizontal mapping
of native full-width screen-space vertices, when positively identified,
is:

```
  x_corrected = 960 + (x_original - 960) * 0.75
```

Do **not** alter Y, z, RHW, texture UVs, color or game-owned vertex
memory. In original 4:3 output the mapping is identity. The formula
is a *future* proposal, not an already enabled game patch.

## Why trace first?

The same Direct3D7 DrawPrimitive entry at caller RVA 0x00102C72
handles world particles, gameplay HUD, comic art and menus. It
cannot uniquely identify the ESC pause panel.

The SHA-gated game code already observes pointer **0x00682950**
as the native gameplay pause context (original pause handler
0x0047BC40); this is separate from the Alt+Tab focus pause.
Its nonzero presence starts passive sampling when available, but is not
alone sufficient to identify individual pause sprites. Under Wine, the
first combined build #728 successfully enabled the diagnostic but did not
record any BEGIN event, while sampled pause_ctx values were zero. The
diagnostic therefore also watches the physical ESC-down edge via the
high bit of GetAsyncKeyState on the existing Flip hook. No input is
consumed, inserted or rerouted; this is a short fallback observation
window, not a pause-detection claim.

The first safe step is therefore read-only identification of
the panel/text/icon signatures: original caller RVAs, D3D primitive
types, FVF, source vertex rectangles, depth/RHW and bound texture
sizes. A follow-up change will add tightly gated 4:3 mapping only
to proven ESC pause components, after examining the logs.

## Test the passive diagnostic (Wine or native Windows)

Use both DLLs from the same build on the diagnostic PR branch.
In `SpiderMan2k1Fix.ini`, enable:

```ini
[Debug]
Log=1
```

Set the game process environment variable
`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1` (in Lutris, configure the
game's **System options → Environment variables**, or export the
variable before launching the game through the same environment).

1. Launch at 1920×1080, **AspectRatio=Auto**.
2. Begin a level and **hold Esc for about half a second** to make the
   physical key-down observable at a Flip. Let the game's original native
   CONTINUE / RESTART LEVEL / QUIT menu open; do not use F10 for this step.
3. Hold this menu for several seconds. Optionally move selection
   to another row, then choose CONTINUE.
4. Quit normally and preserve `SpiderMan2k1Fix*.log`.
5. Search for `PAUSE-NATIVE-PROBE: ESC-EDGE`, `BEGIN`, `FRAME`,
   `caller_rva=`, and `END`. If no ESC-EDGE is present, the physical
   ESC key was not observed in a Flip. If ESC-EDGE appears but BEGIN or
   candidate draws do not, the chosen D3D7 path/filter may be incorrect.

The trace logs up to four Flip generations and at most 160 candidate
2D draws per frame, from a maximum 12-Flip sampling window after each
ESC edge (up to four ESC edges) or when the game-native context is set.
It does not modify rendered coordinates, pause
input, F10, F9, the camera or texture contents. If no BEGIN appears,
the assumed native pause-context gate needs new passive research;
**do not enable a geometry rewrite based on guessing**.

Optional comparison: repeat once in **AspectRatio=4:3** and compare
the same native pause draw signatures with the full-width run. The
screenshots alone establish the desired visual reference, not the
individual reliable vertex signatures.

Finally, remove the environment variable; the diagnostic is
**OFF by default** and requires `Log=1`.
