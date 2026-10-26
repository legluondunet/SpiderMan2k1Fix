# Experimental loose-file modding for PKR resources

Opt-in replacement of resources normally loaded through the original
Spider-Man 2001 game's PKR_ReadFile function, **without changing data.pkr
or Media.pkr**. Windows x86 prototype, unvalidated in gameplay.

## Activation

Quit the game. Add this section ONCE to SpiderMan2k1Fix.ini:

~~~ini
[Modding]
LooseFiles=1
~~~

Default is 0 (disabled): the original PKR reader remains unchanged.
The validator and INI repair recognize and preserve the new boolean setting.

## Resource priority

A game request for AUDIO\wind2.wav or data\AUDIO\wind2.wav tries:

~~~text
Spider-Man/
  SpideyPC.exe
  SpiderMan2k1Fix.dll
  SpiderMan2k1Fix.ini
  winmm.dll
  data.pkr
  Media.pkr
  data/
    AUDIO/
      wind2.wav
~~~

When a loose file exists, is safe, nonempty, readable, and <= 128 MiB,
its raw bytes and length are returned using **the game's native
allocation method**, so original callers free it normally.
If the file is absent, unreadable or refused, **the original PKR
read routine is called unchanged**. Windows filenames are normally
case-insensitive. The game may load some other resources through Bink
or other APIs; they are outside this hook's scope.

**Modding limits:** only resources actually requested through the
native PKR function can be overridden. Adding new file names not known
to the game is not supported. Replacement data must still use a format
understood by the game. No hot-reload; restart after changing files.

**Safety:** the original EXE SHA-256 is checked by SpiderMan2k1Fix
before installation. The detour has an exact 9-byte instruction-site
signature guard. Disk paths are anchored to the EXE directory, reject
absolute and traversal paths, reserved Windows names, and reparse
points (junctions and symlinks). No PKR CRC code is disabled.

## Diagnostic test with wind2.wav

1. Back up old DLLs. Use the Windows x86 build artifact of the draft PR.
2. Put the prepared 200 Hz replacement file at data/AUDIO/wind2.wav.
3. Enable [Debug] Log=1 and [Modding] LooseFiles=1; retain sound effect volume.
4. Restart the game, play the opening 3D scene and verify that the tone
   replaces wind. Look for "LOOSE-FILES: override [...] wind2.wav"
   in the game log to confirm the actual disk loading.
5. Repeat with the silent replacement. Compare the crackle against
   unchanged game PKRs, with no disk file / LooseFiles=0 as baseline.

Caution: a successful build does not prove that this native detour is
runtime-safe. Test only on a backed-up installation. Do not merge without
manual Windows and Wine results.

## Future work

A standalone PKR extraction command is a separate follow-up. It should
reuse a supported archive extractor rather than reimplement PKR decoding
inside the game DLL.
