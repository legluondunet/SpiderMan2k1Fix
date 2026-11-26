# World effects: focused canvas investigation (#147 / #146)

## Evidence and limits

The generic gameplay UI mapping accepts near-zero-depth XYZRHW quads with a
shared original DrawPrimitive caller RVA 0x00102C72. Source comments already
warn that 3D world sprites can share this caller. The current build enables
the common canvas in widescreen after the native HUD signature checks.
A successful candidate match is not proof of HUD ownership. The separate
Hor+ experiment remains disabled. There is no evidence yet that the moving
shadows or missing W spark actually pass through the common canvas.

## Diagnostic build: no rendering changes

Close the game. In SpiderMan2k1Fix.ini set:

    [Debug]
    Log=1
    RenderCanvasTrace=1

Keep AspectRatio=Auto and FPSLimit unchanged for the first session.
Reach the affected scene. For moving shadows, take two or three F9 PNGs from
the same position, including the anomalous state; for the W spark, take a
PNG showing the complete sign when the effect should appear. Provide the
PNG files and the current SpiderMan2k1Fix.log; note level/checkpoint, camera
movement and whether the effect ever appears. An optional short video helps
distinguish animation from displacement.

The diagnostic collects only successful common-canvas transforms, in a fixed
two-Flip ring (128 samples per interval), and dumps the current/previous
interval on a successfully queued gameplay F9 screenshot (maximum eight).
Each sample records the original caller, first-vertex color, depth/RHW,
known native HUD scope, cached Z-enable status, source bounds and mapped
horizontal bounds. It performs no new draw calls, state changes or texture
queries. Flip intervals are not an exact screenshot-to-primitive proof;
omission counts identify truncation. No candidate recorded does not prove a
world effect was never submitted or transformed through another path.

For the comparison session set AspectRatio=4:3, keep the same FPSLimit and
repeat the scene and F9 captures. The common widescreen canvas is not
enabled in 4:3, but other rendering differences also exist; a difference is
a clue, not attribution. Restore Auto and remove RenderCanvasTrace when
finished. No per-level patch, no change to default rendering or save files.
