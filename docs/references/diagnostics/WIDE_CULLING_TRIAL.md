# Issue #181: horizontal native visibility planes (experimental)

## Static evidence from authenticated retail PC executable

SHA-256: 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea.
No original binary distributed or modified on disk.

- 0x4730B2..0x4730D3 constructs the camera-space mirrored XZ normals at
  0x65CED8 / 0x65CEE0. Y components are zero. The native integer normalization
  uses the fixed-point geometry setup, not the patched float at 0x550064.
- 0x4731E5..0x473235 loads and rotates six normals into two 3x3 short arrays
  0x628648 / 0x628620. The second vector load (CALL 0x473204 -> 0x46D8A0)
  receives first 0x65CED0 (vertical), then 0x65CED8 and 0x65CEE0 (horizontal).
  The destination arrays are later copied at 0x4739BC/0x4739C6 to 0x610B40 /
  0x610B60. 0x473A0A calls 0x46FAD0.
- 0x46FAD0 iterates objects; fixed-point dot-product plane rejection writes
  object flag 0x8000 at 0x46FE67, 0x46FEA5, 0x46FEE3, 0x46FF1F,
  0x46FF5C, 0x46FF96; further bounding-box tests at 0x470118..0x470273.
  0x473AA4..0x473AAB tests the same flag in the scene submission loop.
- The existing aspect scalar is read at 0x473504 and 0x49B7CB during
  projection calculation. The native plane construction does not use it.
  This establishes a credible mismatch, not proof it is the only cause of
  all observed pop-in.

## Trial

Opt-in only, with Log=1, ExperimentalWideCulling=1 under [Debug] and
AspectRatio=Auto under [Display]. The exact call is redirected after
retail authentication and signature checks. A local temporary vector
scales |X| by the existing gameplay aspect scalar then normalizes to its
original length. The two mirrored original normals must agree, have Y=0,
positive Z and approximate fixed-point length 4096. Otherwise the native
vector is forwarded unchanged. No writes to the original plane arrays
by the wrapper, no global GTE scale change, no disabling of culling,
no near/far or vertical modification. 4:3 and menu are unchanged.
First eight mapped vectors (or first rejected pair) logged as WIDE-CULL.

Test the same rooftop with slow camera rotation, keeping camera location
as similar as possible. Compare to the existing widescreen recording;
look for formerly missing buildings entering from the actual screen edge
rather than the old central 4:3 boundary. Check for wrongly visible objects,
missing geometry, performance or instability. Disable the option to
restore the baseline; no persistent gameplay change.
