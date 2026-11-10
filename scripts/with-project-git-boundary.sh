#!/bin/sh
# Keep legacy private replay directories from discovering this new backup repo.
# This affects only this invocation; it does not modify Git or SDK settings.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
export GIT_CEILING_DIRECTORIES="$ROOT${GIT_CEILING_DIRECTORIES:+:$GIT_CEILING_DIRECTORIES}"
if [ "$#" -eq 0 ]; then
  printf 'Usage: sh scripts/with-project-git-boundary.sh command [arguments...]\n' >&2
  exit 2
fi
exec "$@"
