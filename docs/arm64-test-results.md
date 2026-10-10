# ARM64 SteamOS / DSTS results — 2026-10-10

## Confirmed

- Managed test suite: 144 passed, none skipped. Tests cover Windows game ABI
  selection, pure IL, native/R2R payload checking, unsupported-target feed refusal,
  update rollback, R2R-only/IL-fallback packages and read-only Proton planning.
- Self-contained linux-arm64 coordinator runs in the actual aarch64 SteamOS VM.
  It downloaded all seven DSTS base dependencies from official upstream releases.
  A repeat fetch respected the update interval. The resulting plan has no missing
  required mods or dependencies, and still reports CanLaunchModded=false.
- Native asset store: sanitizer tests pass for traversal, normalized duplicates,
  DDS aliasing, exact bounds, missing assets and ambiguous signatures.
- Windows x64 ASI and production-callback synthetic executable build with warnings
  treated as errors. The latter passed in ARM64 Proton in 5.27 seconds, exit 0,
  explicit success marker present. It checks real MinHook detours, size/read
  replacement, immutable snapshots after source modification, missing-file vanilla
  fallback, residual-detour inertness and hook removal. No .NET is loaded.
- Read-only inspection of the actual installed DSTS x64 executable found one read
  signature at RVA 0x2ca090 and one size signature at RVA 0x2b9be0. Executable hash
  and static inspection results are retained. This is not ABI or gameplay proof.
- Companion BepisLoader branch: full build_app.sh build passes with existing
  unrelated warnings; six compiled policy cases and eight source regression checks
  pass. Existing bepis.sock protocol and disabled launch flow are preserved.

## Test isolation and limits

The initial fresh-prefix Proton run timed out before test startup. It is not a
hook failure. The corrected run restored the previously verified synthetic
prefix snapshot (SHA-256 b546692f45452c972e30b013a4a7399fed1e92c0ef04fdfce8611b2673837b88),
using existing staged compatibility metadata. A private copy of installed Proton
changed only runinprefix's session initialization, as in earlier Bepis tests.
The original runtime was untouched. Bubblewrap used private PID/network namespaces,
private HOME/compatdata/tmp, Xvfb and software rendering. The live game and prefix
were not mounted. A deadline kills the process group and temporary directories
are removed after collection. No live game/prefix files were changed.

The actual game executable was inspected read-only outside the sandbox. No ASI
was installed in the game, no Steam launch option was changed, and no save loaded.
The native ASI remains experimental and is not activated by BepisLoader.
The Reloaded in-process .NET hosting stall is unresolved. The asset-only route
bypasses that dependency but does not replace arbitrary Reloaded/Ryo mods.

## Reproduction

Managed: `dotnet test src/ReloadedDropIn.Tests -c Release -p:NuGetAudit=false`.
Native portable: compile assets.c + test-assets.c with address/undefined sanitizers.
Windows: `./scripts/build-mvgl-native.sh`.
Guest: `python3 scripts/test-mvgl-proton.py --stage /absolute/staged-inputs`.
The guest test requires the exact pre-existing verified synthetic prefix archive,
compat-metadata and compiled test-hooks.exe; private prefix archives are not
committed. Stage the inputs yourself, preserving their verified hashes. Tests
write only scratch/evidence under the provided stage and new temporary directory.
Static: `python3 scripts/inspect-mvgl-game.py /absolute/game.exe`.

See arm64-test-evidence/ for compact results and artifact hashes. No copyrighted
game binary, private prefix or downloaded third-party binary is committed.
