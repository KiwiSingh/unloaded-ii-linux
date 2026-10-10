# Digimon Story Time Stranger on ARM64 SteamOS / Proton

Target: Steam AppID 1984270, Windows x64 executable, Windows x64 in-process
Reloaded-II/DSTS loaders, running under Proton in an ARM64 SteamOS VM.
The ARM64 Linux tool is a coordinator, not an ARM64 injector. Rebuilding a
Windows hook DLL as a Linux ARM64 library cannot hook this x64 Windows process.

## Host builds and dependency preparation

Build with .NET SDK 9.0.300 (global.json):

```
./scripts/package-linux-cli.sh linux-arm64
./dist/coordinator-linux-arm64/ReloadedDropIn.Cli proton-plan --game-dir /absolute/game/path
./dist/coordinator-linux-arm64/ReloadedDropIn.Cli fetch-base-mods --game-dir /absolute/disposable/game/path
```

`proton-plan` reads the game's PE machine and emits schema-versioned JSON. Exit
0 means the payload ABI is supported for preparation, NOT launch readiness.
`CanLaunchModded` remains false. Unsupported or malformed executables fail.
`fetch-base-mods` downloads directly from official upstream feeds, selecting the
existing Windows x64 asset names. It does not run a game or write Wine AppData.
Do not run native Linux `sync` to produce Windows loader configuration: Linux
paths are not Windows paths. Existing in-process Windows sync retains that job.
The host coordinator does not install the proxy, Reloaded runtime, or loader.

`package-release.sh` remains a Windows x64 package, usable on an ARM64 host only
through compatible Proton translation. PAYLOAD_RID other than win-x64 is rejected.
No Windows ARM64 runtime is substituted for the x64 game runtime.

## Downloaded loaders

RequiredMod records explicitly declare win-x64. Other injected architectures
are rejected before release-feed access. Before an update is committed, the selected
managed payload (existing x64 R2R image, otherwise ModDll) and declared native
x64 payload must exist inside staging and parse as Windows images. Native and R2R images must match x64; pure AnyCPU IL is allowed.
The updater retains its staging/rollback behavior. This validation is ABI checking,
not publisher authentication, sandboxing, or proof that runtime hook code works.

See arm64-dependency-audit.json for upstream source/license metadata. Null or
NOASSERTION license metadata does not authorize a source port or redistribution.
Downloaded dependencies remain upstream binaries; no third-party source is copied
or modified here. DSTS is a managed loader built for the Windows game and its
Reloaded hooks; translating it to ARM64 Linux is not the correct integration.

## BepisLoader integration

The companion BepisLoader branch codex/unloaded-arm64 uses existing bepis.sock
hello/version checks, executable PE inspection, Proton runtime resolution, and
Reloaded inventory. Its Digimon launch check reports win-x64 payloads with a
linux-arm64 coordinator. It sends no arbitrary guest commands and preserves the
existing disabled modded-launch flow. Inventory describes Reloaded installation,
not proof that the Unloaded-II drop-in or any mod ran.

There is no protocol capability to execute this native CLI or attest Unloaded-II
hosting. Adding an unadvertised command or bypassing the cockpit gate would be
unsafe. The preparation CLI can currently be used separately in a disposable tree.

## Runtime blocker

Existing BepisLoader 42A-45/46 diagnostics show native hostfxr initialization but
no completed component entry resolution under ARM64 Proton. Therefore this change
is not a playable modded-game port. Host-native preparation cannot repair that
in-process runtime boundary. A bounded disposable comparison of the official
Setup-Linux.exe workflow against the existing ASI hosting reproducer is still
needed before selecting a runtime workaround. Live game/prefix files and current
Steamac recovery work are untouched by this change.

## Asset-only alternative

See [native MVGL assets](native-mvgl-assets.md) for an experimental Windows x64
ASI that bypasses .NET for prepared loose assets, and [test results](arm64-test-results.md)
for the successful synthetic ARM64 Proton test. Live modded gameplay remains unverified.
