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


## Second field diagnostic: downstream native reads

First field test fa1c267 yielded 33 native Escape downs (FF) and 33 ups
(80), zero same-batch overwrites, three intentionally consumed wake keys.
A down at Flip 1621 after focus resume did not visibly open Pause, while
the next at 1646 did. This narrows the next observation to the downstream
readers, not a proven correction.

No extra INI key: EscapeBatchTrace=1 with Log=1 also arms the existing
signature-checked raw-key and five native 3D mask pass-through wrappers
at Flip 120, independently of F10. Existing wrappers are reused if
already installed. Each native call executes exactly once and returns
unchanged. For up to eight completed polls and four Flip intervals
after a native Escape down, capture raw DIK 1 reads (caller return VA,
mode, inner return VA, result) and native mask reads (callsite, mask,
two arguments, result). Also record latch before/after and native
pause-context candidate. Global order and poll serial correlate records
with ESC-BATCH, even though queues drain separately. No OS key injection,
no native state change, no file logging inside reader callbacks.
128 reader entries per interval, 4096 total, explicit omission counts.
No observed reader is evidence only within the stated window and
successful hook coverage; it does not prove the action was never read.

Repeat normal Escape open/close during focused gameplay and after two
or three Alt+Tab returns. Resume the external-focus pause with Enter
rather than Escape to distinguish wake consumption from a missed native
Pause request. Note any ignored presses, send only the log.
