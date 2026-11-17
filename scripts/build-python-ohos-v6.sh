#!/usr/bin/env bash
# Build the pinned, unmodified CPython sources with the actual OHOS compiler.
# This is runtime preparation, not a claim that Olex2 plugins are integrated.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$ROOT/out/python-runtime-evaluation"
ARCHIVE="$BASE/downloads/Python-3.11.18.tar.xz"
HOST="${PYTHON_BUILD_INTERPRETER:-$BASE/host-build-attempt1/python.exe}"
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
ID="$(date +%Y%m%d-%H%M%S)-$(python3 -c 'import uuid; print(uuid.uuid4().hex[:6])')"
OUTPUT="$BASE/ohos-builds/$ID"
mkdir -p "$OUTPUT/build" "$OUTPUT/toolchain"
cp "$0" "$OUTPUT/build-recipe.sh"
printf '%s\n' "$OUTPUT" > "$BASE/latest-ohos-attempt.txt"
finish() { result=$?; printf '%s\n' "$result" > "$OUTPUT/exit-code.txt"; printf 'Retained Python OHOS build: %s (exit %s)\n' "$OUTPUT" "$result" >&2; }
trap finish EXIT
python3 - "$ARCHIVE" "$OUTPUT" "$HOST" "$SDK" <<'PY'
import hashlib,json,pathlib,subprocess,sys,tarfile
archive,out,host,sdk=map(pathlib.Path,sys.argv[1:])
sha='9120e0a653868f6e3698d36cdd99e5a58903ec9469e448f21d98f7b16d8fa290'
assert hashlib.sha256(archive.read_bytes()).hexdigest()==sha
assert subprocess.check_output([str(host),'-I','-c','import sys; print(".".join(map(str,sys.version_info[:3])))'],text=True).strip()=='3.11.18'
with tarfile.open(archive) as t:t.extractall(out/'source',filter='data')
info={'name':'CPython','version':'3.11.18','archive_sha256':sha,'source_url':'https://www.python.org/ftp/python/3.11.18/Python-3.11.18.tar.xz','source_modified':False,'compiler_target':'aarch64-linux-ohos','configure_host':'aarch64-unknown-linux-musl','configure_host_note':'CPython3.11 only recognizes selected cross-build OS names; actual compiler/sysroot/ELF use OHOS, not a GNU/Linux compiler or runtime.','host_python':str(host),'host_python_sha256':hashlib.sha256(host.read_bytes()).hexdigest(),'compiler_sha256':hashlib.sha256((sdk/'llvm/bin/clang').resolve().read_bytes()).hexdigest(),'full_port_complete':False,'application_integration_complete':False}
(out/'source-and-toolchain.json').write_text(json.dumps(info,indent=2)+'\n')
PY
SOURCE="$OUTPUT/source/Python-3.11.18"
# No pseudo-terminal support is requested: the current target shell observed
# neither /dev/ptmx nor /dev/ptc as readable. This is not a system-wide claim.
cat > "$OUTPUT/config.site" <<'SITE'
ac_cv_file__dev_ptmx=no
ac_cv_file__dev_ptc=no
# The SDK libintl.h exists but declares none of gettext/dgettext/textdomain.
# CPython3.11 otherwise equates header presence with the unavailable API.
ac_cv_header_libintl_h=no
# SDK linux/can{,/raw}.h cannot be combined with sys/socket.h (duplicate
# sockaddr_storage). CPython probes the enum alone and wrongly enables it.
# Keep regular TCP/IP sockets; do not advertise unavailable CAN options.
ac_cv_can_raw_fd_frames=no
ac_cv_can_raw_join_filters=no
SITE
cat > "$OUTPUT/toolchain/ohos-clang" <<'PY'
#!/usr/bin/env python3
import json,os,pathlib,subprocess,sys,time
sdk=pathlib.Path(os.environ['OLEX2_PYTHON_NATIVE_SDK'])
args=sys.argv[1:]
command=[str(sdk/'llvm/bin/clang'),'--target=aarch64-linux-ohos','--sysroot='+str(sdk/'sysroot')]+args
if '-c' in args and '-o' in args:
    obj=args[args.index('-o')+1]
    command+=['-MD','-MF',obj+'.d']
start=time.time()
# CPython3.11 hardcodes a GNU-style platform tag from compiler macros and
# rejects OHOS's real --print-multiarch answer. Decline this optional query
# rather than claim a GNU compiler: retain its real answer in the command log.
# This only controls CPython's legacy extension tag, not compiler/ABI flags.
if args == ['--print-multiarch']:
    actual=subprocess.run(command,capture_output=True,text=True)
    record={'cwd':os.getcwd(),'command':command,'real_returncode':actual.returncode,
            'real_stdout':actual.stdout,'real_stderr':actual.stderr,
            'compatibility_action':'optional_multiarch_query_declined_for_legacy_cpython_platform_tag',
            'returned_stdout':'','returned_status':0}
    fd=os.open(os.environ['OLEX2_PYTHON_COMMAND_LOG'],os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o644)
    try:os.write(fd,(json.dumps(record,ensure_ascii=False)+'\n').encode())
    finally:os.close(fd)
    sys.exit(0)
result=subprocess.run(command)
record={'cwd':os.getcwd(),'command':command,'returncode':result.returncode,'started_at':start}
fd=os.open(os.environ['OLEX2_PYTHON_COMMAND_LOG'],os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o644)
try:os.write(fd,(json.dumps(record,ensure_ascii=False)+'\n').encode())
finally:os.close(fd)
sys.exit(result.returncode)
PY
chmod 755 "$OUTPUT/toolchain/ohos-clang"
export OLEX2_PYTHON_NATIVE_SDK="$SDK" OLEX2_PYTHON_COMMAND_LOG="$OUTPUT/compiler-commands.jsonl"
cd "$OUTPUT/build"
CONFIG_SITE="$OUTPUT/config.site" CC="$OUTPUT/toolchain/ohos-clang" \
 AR="$SDK/llvm/bin/llvm-ar" RANLIB="$SDK/llvm/bin/llvm-ranlib" READELF="$SDK/llvm/bin/llvm-readelf" \
 CFLAGS='-O2 -fPIC' LDFLAGS='-Wl,-z,relro,-z,now' MODULE_BUILDTYPE=static \
 "$SOURCE/configure" --prefix=/olex2-python-runtime --host=aarch64-unknown-linux-musl \
 --build="$("$SOURCE/config.guess")" --with-build-python="$HOST" \
 --enable-shared --with-suffix=.ohos --without-ensurepip --with-pkg-config=no --enable-ipv6 \
 --disable-test-modules > "$OUTPUT/configure.log" 2>&1
# Setup.local is a generated build configuration, not a source patch. Use
# upstream's detected modules and flags, statically linked into libpython.
cp Modules/Setup.stdlib Modules/Setup.local
# GNU make3.81 sees second-resolution timestamps on this Mac; explicitly
# regenerate after Setup.local instead of trusting same-second mtimes.
sh "$SOURCE/Modules/makesetup" -c "$SOURCE/Modules/config.c.in" -s Modules \
 Modules/Setup.local Modules/Setup.bootstrap "$SOURCE/Modules/Setup" > "$OUTPUT/setup-modules.log" 2>&1
mv config.c Modules/config.c
python3 - <<'CHECK'
from pathlib import Path
config=Path('Modules/config.c').read_text()
for module in ['math','_json','_struct','zlib','_decimal','pyexpat','unicodedata','_codecs_cn']:
    assert 'PyInit_'+module in config, module
CHECK
make -j8 python.ohos libpython3.11.a > "$OUTPUT/make.log" 2>&1
"$SDK/llvm/bin/llvm-readelf" -h -d python.ohos libpython3.11.so.1.0 > "$OUTPUT/elf-headers-and-dependencies.txt"
python3 - "$OUTPUT" <<'PY'
import hashlib,json,pathlib,sys,tarfile
out=pathlib.Path(sys.argv[1]);archive=out.parents[1]/'downloads/Python-3.11.18.tar.xz'
with tarfile.open(archive) as t:
    for member in t.getmembers():
        if member.isfile():
            p=out/'source'/member.name
            assert p.read_bytes()==t.extractfile(member).read(),str(p)
files=[]
for name in ['python.ohos','libpython3.11.so.1.0','libpython3.11.a','pyconfig.h','Makefile','Modules/Setup.local','Modules/config.c']:
    p=out/'build'/name
    files.append({'path':'build/'+name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
result={'built':True,'target':'aarch64-linux-ohos','legacy_python_extension_tag':'aarch64-linux-gnu (CPython3.11 configure metadata; not an ABI/third-party-wheel compatibility claim)','source_unmodified_after_build':True,'artifacts':files,'application_integration_complete':False,'target_runtime_executed':False,'full_port_complete':False}
(out/'build-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
PY
