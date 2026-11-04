# Verified original game-action state layout (research, no runtime changes)

Source: exact supported retail PC `SpideyPC.exe`, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`,
offline x86 disassembly. No game binary is committed.

Original function `0x00505720` calls `0x00479540` **20 times**
and passes a distinct pointer to a **16-byte** native action
state record on each call. Fifteen-plus-one records advance
from `0x00661100` in steps of 16. The final four calls
explicitly address `0x00661200`, `0x00661210`,
`0x00661220` and `0x00661230`. The complete range is
`0x00661100`–`0x0066123F`, and a separate global
`0x0066129C` tracks an input-service counter.

The exact 20 state-mask assignments (in call order, starting
at state slot 0) are:

| Slot | Mask | Slot | Mask |
| ---: | ---: | ---: | ---: |
| 0 | `0x0080` | 10 | `0x0001` |
| 1 | `0x0040` | 11 | `0x0002` |
| 2 | `0x0020` | 12 | `0x0000` |
| 3 | `0x0010` | 13 | `0x0000` |
| 4 | `0x0100` | 14 | `0x1000` |
| 5 | `0x0000` | 15 | `0x0000` |
| 6 | `0x0200` | 16 | `0x0010` |
| 7 | `0x0400` | 17 | `0x0080` |
| 8 | `0x0004` | 18 | `0x0040` |
| 9 | `0x0008` | 19 | `0x0020` |

There are **intentional duplicate masks**: slots 3 and 16
both use `0x10`, and slots 0/17, 1/18 and 2/19 also
share masks. They are nevertheless **separate 16-byte
records with independent held/transition counters**. Slots
5, 12, 13 and 15 receive zero. This is the actual
unrolled original function, not an assumed semantic
`Jump/Kick` ordering for every slot.

The portable research-only header `sdl3_original_action_layout.h`
defines the addresses and bitmasks for future comparison
against SDL3 produced HELD states, and the C99 tests
verify their spacing and known repeated masks. No
runtime file includes this header and nothing writes
those addresses.

## Safe next experiment (not yet implemented)

At a strictly identified foreground game/context
boundary, a future read-only monitor could snapshot
only a small subset of the 20 records and their
counter deltas, without calling `0x00505720` a second
time. It must verify the EXE SHA, runtime signatures
and correct thread/context, bound the log, avoid
Bink/movie menu timing and retain the original
DS4/Xbox single owner.

This evidence still does not authorize a generic
SDL3-to-original game command write, nor prove
that Steam Deck or DualShock 3 works yet.

PR #88 remains DRAFT and stacked on the DS4/Xbox
PR #80. No merge without maintainer authorization.
