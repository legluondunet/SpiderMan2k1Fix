# Configuration regression fixtures

These three files are **reference input data for tests**, not files that should
be copied into the game installation or shipped in release ZIPs. The running
game creates its own `SpiderMan2k1Fix.ini` if none exists.

| Fixture | Reason for retaining it |
| --- | --- |
| `SpiderMan2k1Fix.ini` | Complete, manually documented baseline used to check our chosen project defaults |
| `SpiderMan2k1Fix.generated-example.ini` | Snapshot of the generated default configuration used to check output, parsing and repair |
| `SpiderMan2k1Fix.user-clean.ini` | Historical alternate user settings (including different joystick assignment and experimental values), used to verify that the original 252-byte raw image and independently edited options can be preserved |

The first two share nearly the same effective defaults **but intentionally differ
in comments/diagnostic documentation**, so tests currently compare both against
the same factory bytes while also validating their individual text. Removing one
would reduce existing test coverage without an explicit replacement.

The *runtime file name* stays `SpiderMan2k1Fix.ini`, next to the actual game
executable, **not** under `ChatGPT/tests/fixtures/`.
