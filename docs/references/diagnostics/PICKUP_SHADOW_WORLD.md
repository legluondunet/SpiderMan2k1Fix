# Pickup ground shadow world trace (#147)

Authenticated original PE32 SHA-256 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea; no original binary included.

Static findings:
- 0x46AFC0 constructs the pickup with vtable 0x53BC5C; 0x46B352 inserts
  via 0x460260 into list head 0x60FB94, link +0x20. The generic active
  list head 0x60DAB4 may temporarily own an actor. 0x460280 unlinks.
- Pickup live position +8, animation anchor +0x110, ground contact
  +0xB8 (initialized at 0x46B15D..0x46B169). Separate visual pickup
  sprite +0x130 is TrainingPick-Up_01, NOT the shadow.
- 0x461142 calls 0x4605A0 after actor update. Native flag +0x46 bit 8
  controls creation/deletion of shadow +0xCC. 0x46061E selects Shadow2
  (0x409144), 0x460672 calls 0x409400 with ground contact +0xB8,
  normal (0,-4096,0), equal native size +0xD0. Four world corners are
  written at quad +0x10, +0x3C, +0x48, +0x54; vtable 0x53B35C.
  0x46069C updates the original shadow color using height-related fields.
- 0x4097E0 transforms quad world corners using camera-relative native
  integer calculations and a separate floating-point matrix path. Exact
  visual attribution and cause remain unproven.

Trial: [Debug] Log=1, ShadowWorldTrace=1. EscapeBatchTrace=1 may remain
enabled for unrelated passive monitoring. This adds no native hooks or
writes. After existing EXE authentication and exact byte preflight, take
bounded read-only native list snapshots at every 60th gameplay Flip,
and after successful queued F9 captures (not static comics/movies or
native Pause). ReadProcessMemory self only, exact class and read-size
validation, max 128 nodes per list, duplicate/cycle guard, max 16
pickup reports per sample, 120 periodic and 12 F9 samples per session.
No retained dereference, no input changes, no projection changes.
Raw fixed-point world values and native camera coordinate values are
reported separately; center-minus-ground-x4 is the exact sum of corner
coordinates minus four times the ground anchor, avoiding rounding.
Owner address is a session-local correlation hint, not a lifetime ID.
F9 correlates a snapshot with queued capture, not proof of same rendered
frame or eventual successful PNG write.

Test: stand near the question mark and web cartridge, capture F9;
move Spider-Man without collecting them and rotate camera, capture F9
again. A few seconds in each position yields periodic observations.
Send log and two screenshots. If the quad stays anchored in world
space while its apparent position changes, examine projection path,
rather than rewriting object coordinates. If native anchor moves,
investigate its writer. No fix is claimed.


## Field evidence and first projection trial

Session 66d0582: 27 snapshots, 77 verified quad observations; each
observed owner retained unchanged world position, ground anchor and
four corners; center-minus-ground-x4=(0,0,0). F9 images at 945,1100,1524
show displacement roughly proportional to horizontal screen-center
distance, consistent with the successful .75 gameplay FOV scalar.

Authenticated disassembly: 0x46DBC0 computes integer perspective
using 0x54F03C focal length and 0x54F040 logical width/2 (512/2).
At 0x4098E0 and the other three calls the native renderer uses this
fixed-point projection, independent of the float 0x550064.
The renderer copies X/Y from packed output into shorts at 0x654F54.
0x40A021..0x40A046 and 0x40A221..0x40A255 compute X pixel scale from
0x568154 (native output width) / 0x61B5FC (logical width).
0x40A1A9 and 0x40A367 submit both native passes to 0x508550, four
groups of six DWORDs (X,Y,depth,U,V,color), caller cleanup. The callee
copies these into private vertices and performs original clipping.
The earlier floating transform is used for depth, not the submitted X.
This explains the missing horizontal aspect factor; visual validation
still needed. The trial changes four X arguments at these two exact
calls only, using center+scalar*(X-center). All other DWORDs forwarded
bit-for-bit, original function called exactly once. No global
fixed-point projection or world coordinate changes. All world quads
submitted by this native renderer share this correction, not just
the tutorial pickups. No per-level or object-specific workaround.

Enable [Debug] ExperimentalWorldQuadFOV=1 (default off) and Log=1
with AspectRatio=Auto. Keep ShadowWorldTrace=1 and EscapeBatchTrace=1.
Both signatures preflight before writing either call; successful
gameplay FOV, actual width equality, native logical width 512 and
matching fixed projection width, no FMV/Pause context, finite complete
quad required. Original preserved on mismatch, first eight mappings
logged. 4:3 and menu unchanged. No keyboard behavior changes.
Repeat previous screenshots of the same pickup shadows from
different directions, look for correctly anchored shadows and any
world-effect regression; send log and PNGs. Disable experimental
option for baseline comparison if needed. No confirmed fix yet.

## Maintainer validation and clean integration

The maintainer confirmed stable ground anchoring after trial 29c3958 (runtime 1739646), and then compared oval shadow shapes in Auto vs 4:3 in nearly the same scene. Both modes show perspective-flattened shadows; the maintainer explicitly approved the correction on 27 Nov 2026. A prior Dreamcast visual comparison was consistent with this result, but not a matched-camera quantitative test. No shadow texture, opacity, size or world coordinates have been changed. Clean integration is based on #182, enables the identical projection correction by default in Auto independent of Log, and retains ExperimentalWorldQuadFOV=0 as a hidden comparison opt-out. ShadowWorldTrace and EscapeBatchTrace are accepted legacy keys but inactive in this clean branch; their instrumentation remains on #183/#184. The tested Pause bypass is deliberately preserved, so consistency while native Pause redraws the 3D world remains an explicit unverified limitation; no claim of full scene coverage, ultrawide or native Windows gameplay. The previous trial procedure above is retained as historical evidence.
