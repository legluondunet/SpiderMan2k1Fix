# Escape native batch observation (#176)

The original keyboard poll 0x501B80 reads up to 16 20-byte records from
DirectInput. At 0x501C56 it strips previous edge bits; at 0x501C87 it stores
FF for a down, and at 0x501C8F it stores 80 for an up. The original edge
reader 0x50A650 requires FF. A down followed by an up in the same batch
can erase the edge before the action reader sees it; not yet demonstrated
as the cause of the reported misses.

This temporary branch is based on clean culling PR #182. Enable
[Debug] Log=1 and EscapeBatchTrace=1, keep normal FPSLimit and AspectRatio.
At Flip 120 the exact original loop and epilogue are checked; a single
five-byte JMP from 0x501C9B observes only the native Escape records after
the loop and reads the resulting latch. PUSHFD/PUSHAD, balanced cdecl
arguments and the displaced native epilogue restore the original state.
The observer has no input writes, no COM or IAT interception, and does
not log to disk on the poll path. The original recovery branches are
unchanged. A bounded queue is drained on the game's existing Flip thread.
Down/up bitmasks encode original event indices; no other keys collected.
The OS high bit and latch are sampled on Flip, not a physical event log.
Omitted counts and report caps are explicit. Native polls that return
before reaching the epilogue are not captured.

Test normal gameplay, tap Escape normally, including a few short taps.
Alternate opening and closing Pause, leave the game focused. If an appui
is ignored, note the approximate order/time and continue a few times.
Send the session log; no screenshots or video required. A nonzero
down-overwritten together with latch=80 documents a lost native press
edge in that batch, not by itself a complete pause-action attribution.
This diagnostic does not change any key semantics.
