#!/usr/bin/env bash
# Build the host-native coordinator. Windows game payloads stay win-x64.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RID="${1:-linux-arm64}"
case "$RID" in linux-arm64|linux-x64) ;; *) echo "Unsupported coordinator RID: $RID" >&2; exit 2 ;; esac
OUT="$ROOT/dist/coordinator-$RID"
dotnet publish "$ROOT/src/ReloadedDropIn.Cli" -c Release -r "$RID" --self-contained true -o "$OUT" -p:NuGetAudit=false
cp "$ROOT/LICENSE" "$OUT/LICENSE"
cp "$ROOT/docs/arm64-proton.md" "$OUT/ARM64-PROTON.md"
# Carry the exact runtime pack's notices for the self-contained distribution.
ASSETS="$ROOT/src/ReloadedDropIn.Cli/obj/project.assets.json"
RUNTIME_PACKAGE=$(python3 - "$ASSETS" "$RID" <<'PYLICENSE'
import json, pathlib, sys
assets = json.load(open(sys.argv[1]))
rid = sys.argv[2]
download = next(x for framework in assets['project']['frameworks'].values()
                for x in framework.get('downloadDependencies', [])
                if x['name'].lower() == 'microsoft.netcore.app.runtime.' + rid)
version = download['version'].strip('[]').split(',')[0].strip()
directory = pathlib.Path(assets['project']['restore']['packagesPath']) / download['name'].lower() / version
if not (directory / 'LICENSE.TXT').is_file():
    raise SystemExit('Runtime pack license directory not found')
print(directory)
PYLICENSE
)
mkdir -p "$OUT/licenses"
cp "$RUNTIME_PACKAGE/LICENSE.TXT" "$OUT/licenses/DOTNET-LICENSE.TXT"
cp "$RUNTIME_PACKAGE/THIRD-PARTY-NOTICES.TXT" "$OUT/licenses/DOTNET-THIRD-PARTY-NOTICES.TXT"
cp "$ROOT/scripts/licenses/SharpCompress.txt" "$OUT/licenses/SharpCompress.txt"
# This artifact intentionally contains no proxy, runtime-win-arm64 or native Linux injector.
