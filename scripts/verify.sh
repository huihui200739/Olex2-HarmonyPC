#!/usr/bin/env bash
# 一条命令验证：来源核对 -> 原生构建与冒烟测试 -> 未签名 debug HAP
# 用法：scripts/verify.sh
#       VERIFY_SKIP_APP=1 scripts/verify.sh   跳过 HAP 构建
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[1/3] 来源核对：上游 commit、补丁集合、许可证"
python3 scripts/check-source.py

echo "[2/3] 原生构建 + 冒烟测试（host）"
scripts/build-baseline.sh host

if [[ "${VERIFY_SKIP_APP:-0}" == "1" ]]; then
  echo "[3/3] 已跳过 HAP 构建（VERIFY_SKIP_APP=1）"
  exit 0
fi

echo "[3/3] 构建未签名 debug HAP"
scripts/build-app.sh
