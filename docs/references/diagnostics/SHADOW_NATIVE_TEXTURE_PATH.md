# Shadow2 native texture path investigation

Field session 84d81b8: named native resource descriptor at 006ADAD4; two tagged draw samples show queue-material == actual bound D3D surface, both absent from the first sixteen descriptor DWORDs. The observed candidate is A4RGB, 64x64, linear filtering already enabled. No named-resource identity established.

The original executable is not included in the repository. Extend the existing opt-in ShadowWorldTrace diagnostic with bounded local byte windows of the exact source-referenced named resource constructor and lookup, quad transform/submit, native queue record builder and D3D queue flush. These are inspection windows, not proven full function boundaries. The native SHA check and six original signatures already gate this code. Collect before installing the quad CALL hooks, via ReadProcessMemory only. The original game bytes are not committed or redistributed; they exist only in the owner's session log.

Track the scalar at 00628614 (referenced by the authenticated native submit context), before/after original submit and in bounded synchronous draws, and compare with queue and texture IDs. These correlations are not an authorization to change rendering. Retain the prior strict draw guard and keep ExperimentalSoftShadows=0.

Prepare standalone alpha reconstruction for future integration: 64x64 A4 values -> 256x256 A8 with separable monotone cubic interpolation and clamped texel-center sampling. No RGB/UV/texture or game files modified. The algorithm is tested independently and NOT called by the runtime. Actual high-resolution rendering must wait until the source mapping and original blend/texture format are established.

Keep the prior Escape edge trial, ExperimentalEscapeEdgeRecovery=1. Use Log=1, EscapeBatchTrace=1, ShadowWorldTrace=1, ExperimentalWorldQuadFOV=1, ExperimentalSoftShadows=0. One normal session, F9 near a pickup, several ordinary ESC opens/closes. The code windows are collected at startup; send only the log if the graphics are unchanged.
