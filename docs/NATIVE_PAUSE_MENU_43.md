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
record any BEGIN event, while sampled pause_ctx values were zero. The next successful Wine log
`SpiderMan2k1Fix(20261024-013833).log` showed ESC-EDGE at Flip 257,
but the first 160 matching triangles in frames 1–4 were nearly identical
world geometry from `caller_rva=0x00103389`. This is **not** a safe
pause-menu signature; the initial cap hid later primitive calls. The
diagnostic therefore also watches the physical ESC-down edge via the
high bit of GetAsyncKeyState on the existing Flip hook. No input is
consumed, inserted or rerouted; this is a short fallback observation
window, not a pause-detection claim.

The build #730 Wine log `SpiderMan2k1Fix(20261024-055402).log`
provides a useful temporal comparison: `ESC-EDGE` at Flip 204,
**3268** D3D candidates per frame in samples 1–4, then **3316**
candidates per frame from frame 5 through frame 20. That is **48 primitives**
appearing just after ESC. The final sampled primitives are four-vertex
2D quads from caller RVA `0x00102C72`, but the 1-in-64 sampling still
misses almost all of the new group. This count difference is evidence
of a new render batch **correlated with ESC**, not yet proof of which
quads are the native PAUSED ribbon/text/selector.

To test that hypothesis, the next diagnostic logs **every candidate
from draw 3200 through draw 3400**, along with the first eight and
every 64th candidate elsewhere, in frames 1, 5, 12 and 20. This
catches the last 48 candidates while preserving the same bounded
window, disabled-by-default opt-in and strict read-only operation.
No gameplay HUD or native pause vertices are transformed.

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
5. Search for `PAUSE-NATIVE-PROBE: ESC-EDGE`, `BEGIN`,
   `SAMPLE-FRAME`, `FRAME-END`, `caller_rva=` and `END`.
   If no ESC-EDGE is present, the physical
   ESC key was not observed in a Flip. If ESC-EDGE appears but BEGIN or
   candidate draws do not, the chosen D3D7 path/filter may be incorrect.

The diagnostic observes a maximum **24-Flip window** after each of
up to four ESC edges. It counts eligible geometry candidates in the
first 20 frames and logs `FRAME-END candidates=` for each frame.
It records bounded details at **sample frames 1, 5, 12 and 20**:
the first eight candidates, every 64th candidate, **and every
candidate from draw 3200 through draw 3400**, up to 8192.
This reveals the extra end-of-frame primitives without logging every world
triangle. The early frame's x/y pairs may be **projected 3D scene
geometry, not menu positions**; do not apply a 4:3 transform from
these readings until the overlay has been independently identified.
It does not modify rendered coordinates, pause input, F10, F9,
the camera or texture contents. If no BEGIN appears,
the assumed native pause-context gate needs new passive research;
**do not enable a geometry rewrite based on guessing**.

Optional comparison: repeat once in **AspectRatio=4:3** and compare
the same native pause draw signatures with the full-width run. The
screenshots alone establish the desired visual reference, not the
individual reliable vertex signatures.

Finally, remove the environment variable; the diagnostic is
**OFF by default** and requires `Log=1`.
