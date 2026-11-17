#!/usr/bin/env bash
# 一条命令验证：补丁完整性 -> 上游核对（可选） -> 原生构建与冒烟 -> 未签名 debug HAP
# 用法：scripts/verify.sh
#       VERIFY_SKIP_APP=1 scripts/verify.sh   跳过 HAP 构建
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[1/4] 补丁完整性核对（source.lock.json）"
python3 - <<'PY'
import hashlib, json, sys
lock = json.load(open('source.lock.json'))
bad = []
for r in lock['patches']:
    h = hashlib.sha256(open(r['file'], 'rb').read()).hexdigest()
    if h != r['sha256']:
        bad.append(r['file'])
print('  补丁 %d 份，哈希不符 %d 份' % (len(lock['patches']), len(bad)))
if bad:
    print('  不符：' + ', '.join(bad[:5]))
    sys.exit(1)
PY

echo "[2/4] 上游 commit 与逐字节重放核对"
if [[ -d upstream/.git ]]; then
  python3 scripts/check-source.py
else
  echo "  upstream/ 不含 .git（镜像或 tarball 克隆），跳过 commit 级核对与补丁重放"
  echo "  需要完整核对时，先把 upstream/ 恢复成上游仓库再重跑本脚本"
fi

echo "[3/4] 原生构建 + 冒烟测试（host）"
scripts/build-baseline.sh host

if [[ "${VERIFY_SKIP_APP:-0}" == "1" ]]; then
  echo "[4/4] 已跳过 HAP 构建（VERIFY_SKIP_APP=1）"
  exit 0
fi

echo "[4/4] 构建未签名 debug HAP"
scripts/build-app.sh
