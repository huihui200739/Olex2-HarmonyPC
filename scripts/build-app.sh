#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEVECO="${DEVECO_HOME:-/Applications/DevEco-Studio.app/Contents}"
export NODE_HOME="$DEVECO/tools/node"
export DEVECO_SDK_HOME="$DEVECO/sdk"
export PATH="$NODE_HOME/bin:$DEVECO/tools/ohpm/bin:$PATH"
# Hvigor rejects non-ASCII project paths. Build a private source snapshot; never
# rename the user's workspace or modify the installed SDK to avoid this limit.
python3 "$ROOT/scripts/check-source.py"
python3 "$ROOT/scripts/vendor_sources.py"
STAGE="$(mktemp -d /tmp/olex2-harmonypc.XXXXXXXX)"
cleanup() {
  result=$?
  if (( result != 0 )); then
    mkdir -p "$ROOT/out/app/failed-stages"
    mv "$STAGE" "$ROOT/out/app/failed-stages/$(basename "$STAGE")"
    printf 'Retained failed build snapshot under out/app/failed-stages/\n' >&2
  else
    rm -rf "$STAGE"
  fi
}
trap cleanup EXIT
for directory in app cmake native upstream vendor; do
  if [[ "$directory" == vendor ]]; then
    # Vendor has a genuine spec/build FILE: app-output filters must not drop it.
    rsync -a "$ROOT/$directory" "$STAGE/"
  else
    rsync -a --exclude=.git --exclude=.hvigor --exclude=oh_modules --exclude=build --exclude=.cxx "$ROOT/$directory" "$STAGE/"
  fi
done
cp "$ROOT/vendor.lock.json" "$STAGE/vendor.lock.json"
cd "$STAGE/app"
"$DEVECO/tools/ohpm/bin/ohpm" install
SDK="$DEVECO/sdk/default/openharmony/native"
INPUTS="$STAGE/app/entry/src/main/resources/rawfile/OLEX2-BUILD-INPUTS.json"
python3 "$ROOT/scripts/build_provenance_graphics.py" capture --stage "$STAGE" --workspace "$ROOT" --sdk "$SDK" --output "$INPUTS" \
  --tool "clang=$SDK/llvm/bin/clang" --tool "clang++=$SDK/llvm/bin/clang++" --tool "cmake=$SDK/build-tools/cmake/bin/cmake" \
  --tool "ld.lld=$SDK/llvm/bin/ld.lld" --tool "llvm-strip=$SDK/llvm/bin/llvm-strip" \
  --tool "python=$(command -v python3)" --tool "ninja=$SDK/build-tools/cmake/bin/ninja" --tool "node=$NODE_HOME/bin/node" \
  --tool "hvigor=$DEVECO/tools/hvigor/bin/hvigorw" --tool "ohpm=$DEVECO/tools/ohpm/bin/ohpm" \
  --tool "hvigor-package=$DEVECO/tools/hvigor/hvigor/package.json" \
  --tool "hvigor-ohos-package=$DEVECO/tools/hvigor/hvigor-ohos-plugin/package.json"
"$DEVECO/tools/hvigor/bin/hvigorw" --mode module -p product=default -p module=entry@default -p buildMode=debug assembleHap --no-daemon
OUTPUT="$STAGE/app/entry/build/default/outputs/default/entry-default-unsigned.hap"
test -f "$OUTPUT"
mkdir -p "$ROOT/out/app"
# Finalize and check inside the private snapshot before replacing the retained
# package. A failed source/dependency check must not publish a new HAP.
ID="$(date +%Y%m%d-%H%M%S)-$(python3 -c 'import uuid;print(uuid.uuid4().hex[:6])')"
BUILD_RECORD="$ROOT/out/app/builds/$ID"
mkdir -p "$BUILD_RECORD"
PROVENANCE="$BUILD_RECORD/entry-default-unsigned.provenance.json"
python3 "$ROOT/scripts/build_provenance_graphics.py" finalize --stage "$STAGE" --sdk "$SDK" \
  --ninja "$SDK/build-tools/cmake/bin/ninja" --hap "$OUTPUT" --inputs "$INPUTS" --output "$PROVENANCE" --evidence-directory "$BUILD_RECORD/build-evidence"
python3 "$ROOT/scripts/build_provenance.py" verify --hap "$OUTPUT" --provenance "$PROVENANCE" --workspace "$ROOT"
cp "$OUTPUT" "$BUILD_RECORD/entry-default-unsigned.hap"
printf '%s\n' "$BUILD_RECORD" > "$ROOT/out/app/latest-build-directory.txt"
# The conventional package path must remain independently verifiable too.
# Preserve any previous published evidence; each unique build is also retained.
if [[ -e "$ROOT/out/app/build-evidence" ]]; then
  mkdir -p "$ROOT/out/app/published-evidence-history"
  mv "$ROOT/out/app/build-evidence" "$ROOT/out/app/published-evidence-history/before-$ID"
fi
cp -R "$BUILD_RECORD/build-evidence" "$ROOT/out/app/build-evidence"
cp "$PROVENANCE" "$ROOT/out/app/entry-default-unsigned.provenance.json"
cp "$OUTPUT" "$ROOT/out/app/entry-default-unsigned.hap"
shasum -a 256 "$ROOT/out/app/entry-default-unsigned.hap"
printf 'Debug HAP: %s\n' "$ROOT/out/app/entry-default-unsigned.hap"
