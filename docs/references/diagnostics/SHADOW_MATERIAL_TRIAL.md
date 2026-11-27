# Dynamic shadow material trial

Based on field-validated world-quad trial #184 (29c3958); preserves opt-in projection and passive Escape trace. Named native Shadow2 resource is loaded by the signature-verified constructor at 0x409144 and written at quad+0x60. Read the value from bounded pickup snapshots. This is not yet proof that the D3D queue uses the same representation.

Observe queue count around two already authenticated world-quad submissions, tag new records with the independently documented 240-byte native queue format, then verify exact record, material, four original vertices and same Flip on actual original DrawPrimitive caller 0x102C72. Compare with recent native Shadow2 material observations. Report actual texture format and filtering state.

Optional first visual trial: only if the material matches and bound texture is RGB with alpha pixels, blend enabled, alpha test off, no source color key, and original filter state is point/linear, temporarily use linear magnification/minification for that draw, then restore both original values. No additional draws, geometry or texture writes, no persistent changes. If no match, unsupported format, or already linear there may be no visual difference. Any restoration failure disables further changes and is logged. Matching material equivalence and visual outcome must be confirmed on hardware; do not merge. A higher-resolution generated shadow mask is a separate subsequent step, not yet implemented.

Test in Auto with [Debug] Log=1, ExperimentalWorldQuadFOV=1, ShadowWorldTrace=1, EscapeBatchTrace=1, ExperimentalSoftShadows=1. Wait at least two seconds near pickups, compare screenshots at near and far distances, send log. Set ExperimentalSoftShadows=0 to compare. Original game files unchanged.
