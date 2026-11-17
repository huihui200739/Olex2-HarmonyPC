#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HERE="$ROOT/out/gx-application-evaluation"
SOURCE="${GX_APP_SOURCE:-$ROOT}"
PROVENANCE_TOOL="$ROOT/scripts/build_provenance_ffi_v1.py"
DEVECO=/Applications/DevEco-Studio.app/Contents
export NODE_HOME="$DEVECO/tools/node" DEVECO_SDK_HOME="$DEVECO/sdk"
export PATH="$NODE_HOME/bin:$DEVECO/tools/ohpm/bin:$PATH"
python3 "$ROOT/scripts/vendor_sources.py" --help > "$HERE/vendor-tool-help.log"
STAGE="$(mktemp -d /tmp/olex2-gx-app-evaluation.XXXXXXXX)"
ID="$(date +%Y%m%d-%H%M%S)-$(python3 -c 'import uuid;print(uuid.uuid4().hex[:6])')"
OUTPUT="$HERE/app-builds/$ID"
mkdir -p "$OUTPUT"
printf '%s\n' "$STAGE" > "$OUTPUT/build-stage.txt"
finish() {
  result=$?
  # Retain each source/build snapshot, including failed candidates.
  mv "$STAGE" "$OUTPUT/stage"
  printf 'Retained candidate snapshot: %s\n' "$OUTPUT" >&2
}
trap finish EXIT
for directory in app cmake native upstream vendor; do
  if [[ "$directory" == vendor ]]; then rsync -a "$SOURCE/$directory" "$STAGE/";
  else rsync -a --exclude=.git --exclude=.hvigor --exclude=oh_modules --exclude=build --exclude=.cxx "$SOURCE/$directory" "$STAGE/"; fi
done
# 真实配方随 stage 保留：candidate 脚本写进 stage，不覆盖工作树里的 build-app.sh。
mkdir -p "$STAGE/scripts"
cp "$ROOT/scripts/build-app-candidate-ffi-v1.sh" "$STAGE/scripts/build-app.sh"
cp "$SOURCE/vendor.lock.json" "$STAGE/"
cd "$STAGE/app"
"$DEVECO/tools/ohpm/bin/ohpm" install
SDK="$DEVECO/sdk/default/openharmony/native"
INPUTS="$STAGE/app/entry/src/main/resources/rawfile/OLEX2-BUILD-INPUTS.json"
python3 "$PROVENANCE_TOOL" capture --stage "$STAGE" --workspace "$SOURCE" --sdk "$SDK" --output "$INPUTS" \
  --tool "llvm-ar=$SDK/llvm/bin/llvm-ar" --tool "clang=$SDK/llvm/bin/clang" --tool "clang++=$SDK/llvm/bin/clang++" --tool "cmake=$SDK/build-tools/cmake/bin/cmake" \
  --tool "ld.lld=$SDK/llvm/bin/ld.lld" --tool "llvm-strip=$SDK/llvm/bin/llvm-strip" \
  --tool "python=$(command -v python3)" --tool "ninja=$SDK/build-tools/cmake/bin/ninja" --tool "node=$NODE_HOME/bin/node" \
  --tool "hvigor=$DEVECO/tools/hvigor/bin/hvigorw" --tool "ohpm=$DEVECO/tools/ohpm/bin/ohpm" \
  --tool "hvigor-package=$DEVECO/tools/hvigor/hvigor/package.json" \
  --tool "hvigor-ohos-package=$DEVECO/tools/hvigor/hvigor-ohos-plugin/package.json"
"$DEVECO/tools/hvigor/bin/hvigorw" --mode module -p product=default -p module=entry@default -p buildMode=debug assembleHap --no-daemon
HAP="$STAGE/app/entry/build/default/outputs/default/entry-default-unsigned.hap"
python3 "$PROVENANCE_TOOL" finalize --stage "$STAGE" --sdk "$SDK" --ninja "$SDK/build-tools/cmake/bin/ninja" --hap "$HAP" --inputs "$INPUTS" --output "$OUTPUT/entry-default-unsigned.provenance.json" --evidence-directory "$OUTPUT/build-evidence"
cp "$HAP" "$OUTPUT/entry-default-unsigned.hap"
python3 "$PROVENANCE_TOOL" verify --hap "$OUTPUT/entry-default-unsigned.hap" --provenance "$OUTPUT/entry-default-unsigned.provenance.json" --workspace "$SOURCE"
printf '%s\n' "$OUTPUT" > "$HERE/latest-app-build.txt"
shasum -a 256 "$OUTPUT/entry-default-unsigned.hap"
