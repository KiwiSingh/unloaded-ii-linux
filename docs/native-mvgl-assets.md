# Experimental DSTS asset-only ASI

This is an independent, narrow Windows x64 implementation of loose-asset
replacement for Digimon Story Time Stranger under Proton on ARM64 SteamOS.
It does not host .NET, use Reloaded-II APIs, or rewrite original MVGL archives.
It does not support arbitrary Reloaded mods, Ryo scripts, executable patches,
file processors, or archive import. It targets already prepared raw loose files.

The game requests an asset size and then bytes. The callbacks use an immutable
snapshot of the selected loose-file root for both requests. Missing replacements
use the original game functions. Case and slash variants are normalized; `.img`
requests may select `.dds` replacements. Copies require an exact size match and
never exceed the supplied capacity. Files are snapshotted before either hook is
activated so editing them mid-game cannot change the reported size versus data.
Limits: 4096 entries, 64 MiB per file, 256 MiB total, depth 32. Duplicate normalized
paths, traversal, non-ASCII paths, reparse entries and incomplete snapshots fail.
These limits favor predictable fail-closed behavior over general mod support.

## Build

```
./scripts/build-mvgl-native.sh
```

Requires x86_64-w64-mingw32-gcc. Outputs `dist/mvgl-native/bepis-mvgl.asi` and
`test-hooks.exe`. ARM64 Linux is the host; the ASI must remain Windows x64.
MinHook v1.3.4 is vendored with its license and source, commit
c3fcafdc10146beb5919319d0683e44e3c30d537. The instruction signatures come from
MIT-licensed RyoTune/DSTS.ModLoader; its license is included separately. No
MVGL.FileLoader implementation source is copied. ABI and path behavior were
inspected as interoperability facts; this is not a wholesale source port.

## Activation is experimental and gated

The ASI exports InitializeASI for an existing Windows x64 Ultimate ASI Loader.
It does nothing in DllMain. It requires both explicit environment values:

- BEPIS_MVGL_ASSET_ROOT: absolute Windows drive path to one prepared loose-file root
  (for FMC Eyes Green, the folder containing `app_0`, inside `dsts-loader`).
- BEPIS_MVGL_EXE_SHA256: lowercase SHA-256 of the exact game executable.

It independently verifies the on-disk hash and exact executable basename,
Windows x64 machine, and unique size/read signatures in executable sections.
It creates both hooks before enabling either; installation failure rolls them
back. It has no automatic download, update or activation path. Do not combine
this ASI with Reloaded/MVGL hooks on the same functions.

BepisLoader continues to block modded launch. There is no automatically attested
installation capability for this experiment. The synthetic test checks the real
production callbacks and MinHook against mock x64 functions, including vanilla
fallback, immutable snapshot behavior and hook removal. It does not establish
that the game's ABI, translated code invalidation or DDS format are compatible.

## Required live validation before declaring success

Use the current recovery/backup flow without weakening it. First inspect the
exact game executable and require one match per signature. Validate loader-only
startup in a bounded run. Then test one raw cosmetic replacement (e.g. FMC Eyes
Green), compare in-game output, and prove restoring the original launch options
and removing the experimental ASI restores vanilla behavior. No save loading is
needed. Actual gameplay/mod success has not been established by synthetic tests.

The FMC Eyes Green texture was confirmed visually in the live ARM64 SteamOS/x64 Proton game on 2026-10-10, with a matching native replacement log. BepisLoader's installer uses this as its first adapter behind game-agnostic asset-mod controls. It requires the companion checked guest publication capability and manual Steam launch setting. Full managed Reloaded-II compatibility remains unverified.
