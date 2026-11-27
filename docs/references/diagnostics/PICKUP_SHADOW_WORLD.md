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
